#include "libscale/manager.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "libscale/calculator.hpp"
#include "libscale/database.hpp"
#include "libscale/device.hpp"
#include "libscale/types.hpp"
#include "libscale/worker.hpp"

namespace libscale {

HealthManager::HealthManager(IDatabaseManager& database, IScaleDevice& device,
                             std::unique_ptr<IBioImpedanceCalculator> calculator)
    : m_database(database),
      m_device(device),
      m_calculator(calculator ? std::move(calculator)
                              : std::make_unique<BioImpedanceCalculatorV1>()),
      m_worker(std::make_unique<SyncWorker>(device, *m_calculator)) {}

HealthManager::~HealthManager() = default;

auto HealthManager::get_users() const -> std::vector<UserProfile> {
    auto users = m_database.get_users();
    const std::string dev_key = m_device.get_device_key();
    auto dev_opt = m_database.get_device_by_key(dev_key);

    if (dev_opt) {
        for (auto& user : users) {
            auto mappings = m_database.get_device_user_mappings(user.id);
            bool mapped = false;
            for (const auto& mapping : mappings) {
                if (mapping.device_id == dev_opt->id) {
                    user.slot_id = mapping.slot_id;
                    user.device_user_id = mapping.device_user_id;
                    mapped = true;
                    break;
                }
            }
            if (!mapped) {
                user.slot_id = 0;
                user.device_user_id = "";
            }
        }
    } else {
        for (auto& user : users) {
            user.slot_id = 0;
            user.device_user_id = "";
        }
    }
    return users;
}

void HealthManager::update_user(const UserProfile& user) {
    m_database.update_user(user);
}

void HealthManager::recalculate_measurements(int user_id) {
    auto users = get_users();
    auto user_iterator = std::ranges::find_if(
        users, [user_id](const UserProfile& user_profile) { return user_profile.id == user_id; });
    if (user_iterator == users.end()) {
        return;
    }
    const auto& user = *user_iterator;

    auto measurements = m_database.get_measurements(user_id);
    for (const auto& measurement : measurements) {
        auto raw_opt = m_database.get_scale_raw_data(measurement.id);
        if (raw_opt) {
            RawMeasurement raw;
            raw.weight_raw = raw_opt->weight_raw;
            raw.imp5k = raw_opt->imp5k;
            raw.imp50k = raw_opt->imp50k;

            ImpedancePathway pathway = ImpedancePathway::HandToFoot;
            if (measurement.device_id) {
                auto dev_opt = m_database.get_device(*measurement.device_id);
                if (dev_opt) {
                    pathway = dev_opt->pathway;
                }
            }

            auto new_meas_opt = m_calculator->calculate(raw, user, pathway);
            if (new_meas_opt) {
                auto new_meas = *new_meas_opt;
                new_meas.id = measurement.id;
                new_meas.date = measurement.date;
                new_meas.device_id = measurement.device_id;
                m_database.add_measurement(new_meas);
            }
        }
    }
}

void HealthManager::delete_user(int user_id) {
    m_database.delete_user(user_id);
}

auto HealthManager::get_measurements(int user_id) const -> std::vector<Measurement> {
    return m_database.get_measurements(user_id);
}

auto HealthManager::get_measurements_range(int user_id, std::chrono::year_month_day start_date,
                                           std::chrono::year_month_day end_date) const
    -> std::vector<Measurement> {
    return m_database.get_measurements_range(user_id, start_date, end_date);
}

void HealthManager::start_sync(std::function<void(SyncResult)> callback) {
    const std::string dev_key = m_device.get_device_key();
    auto dev_opt = m_database.get_device_by_key(dev_key);
    int device_id = 0;
    if (!dev_opt) {
        Device new_dev;
        new_dev.name = m_device.get_device_name();
        new_dev.device_key = dev_key;
        new_dev.pathway = m_device.get_impedance_pathway();
        device_id = m_database.add_device(new_dev);
    } else {
        device_id = dev_opt->id;
    }

    auto existing_users = get_users();

    // Safety: Capture dependencies carefully. Since m_worker is joined in destructor,
    // 'this' is safe as long as the worker thread is running.
    auto wrapped_callback = [this, device_id,
                             callback_moved = std::move(callback)](const SyncResult& result) {
        if (result.success) {
            process_sync_results(device_id, result);
        }

        if (callback_moved) {
            callback_moved(result);
        }
    };

    m_worker->start_sync(std::move(existing_users), std::move(wrapped_callback));
}

void HealthManager::process_sync_results(int device_id, const SyncResult& result) {
    // 1. Reconcile users discovered on scale
    auto all_existing_users = m_database.get_users();

    for (const auto& user : result.new_users) {
        auto it_existing = std::ranges::find_if(
            all_existing_users,
            [&](const UserProfile& local_user) { return local_user.name == user.name; });

        if (it_existing != all_existing_users.end()) {
            int const user_id = it_existing->id;
            const auto existing_mappings = m_database.get_device_user_mappings(user_id);
            bool const already_mapped = std::ranges::any_of(
                existing_mappings, [&](const DeviceUserMapping& device_mapping) {
                    return device_mapping.device_id == device_id;
                });
            if (!already_mapped) {
                // Register DeviceUserMapping for the existing user
                DeviceUserMapping mapping;
                mapping.user_id = user_id;
                mapping.device_id = device_id;
                mapping.slot_id = user.slot_id;
                mapping.device_user_id = user.device_user_id;
                m_database.add_device_user_mapping(mapping);
            }
        }
    }

    // 2. Map scale user IDs to database user IDs and save measurements
    auto all_users = get_users();
    std::unordered_map<std::string, int> device_id_to_db_id;
    for (const auto& user : all_users) {
        if (!user.device_user_id.empty()) {
            device_id_to_db_id[user.device_user_id] = user.id;
        }
    }

    for (const auto& sync_meas : result.measurements) {
        auto it_user = device_id_to_db_id.find(sync_meas.device_user_id);
        if (it_user != device_id_to_db_id.end()) {
            Measurement measurement = sync_meas.measurement;
            measurement.user_id = it_user->second;
            measurement.device_id = device_id;
            m_database.add_measurement(measurement);
        }
    }
}

void HealthManager::associate_scale_user(const UserProfile& scale_user, std::optional<int> user_id,
                                         const SyncResult& result) {
    const std::string dev_key = m_device.get_device_key();
    auto dev_opt = m_database.get_device_by_key(dev_key);
    const int device_id = [&]() {
        if (!dev_opt) {
            Device new_dev;
            new_dev.name = m_device.get_device_name();
            new_dev.device_key = dev_key;
            new_dev.pathway = m_device.get_impedance_pathway();
            return m_database.add_device(new_dev);
        }
        return dev_opt->id;
    }();

    const int target_user_id = user_id ? *user_id : m_database.add_user(scale_user);

    // Register DeviceUserMapping for this user
    DeviceUserMapping mapping;
    mapping.user_id = target_user_id;
    mapping.device_id = device_id;
    mapping.slot_id = scale_user.slot_id;
    mapping.device_user_id = scale_user.device_user_id;
    m_database.add_device_user_mapping(mapping);

    // Save their measurements
    for (const auto& sync_meas : result.measurements) {
        if (sync_meas.device_user_id == scale_user.device_user_id) {
            Measurement measurement = sync_meas.measurement;
            measurement.user_id = target_user_id;
            measurement.device_id = device_id;
            m_database.add_measurement(measurement);
        }
    }
}

void HealthManager::stop_sync() {
    m_worker->stop();
}

}  // namespace libscale
