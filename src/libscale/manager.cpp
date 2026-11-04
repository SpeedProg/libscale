#include "libscale/manager.hpp"

#include <bits/chrono.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

auto HealthManager::get_users() const -> std::vector<UserProfile> {
    std::scoped_lock const lock(m_mutex);
    return get_users_locked();
}

auto HealthManager::get_users_locked() const -> std::vector<UserProfile> {
    auto users = m_database.get_users();
    std::string_view const dev_key = m_device.get_device_key();
    auto dev_opt = m_database.get_device_by_key(std::string{dev_key});

    // Fetch all mappings for the connected device once instead of querying per user.
    std::unordered_map<int, DeviceUserMapping> mappings_by_user;
    if (dev_opt) {
        for (const auto& mapping : m_database.get_device_user_mappings_by_device(dev_opt->id)) {
            mappings_by_user.insert_or_assign(mapping.user_id, mapping);
        }
    }

    for (auto& user : users) {
        auto const mapping_it = mappings_by_user.find(user.id);
        if (mapping_it != mappings_by_user.end()) {
            user.slot_id = mapping_it->second.slot_id;
            user.device_user_id = mapping_it->second.device_user_id;
        } else {
            user.slot_id = 0;
            user.device_user_id.clear();
        }
    }
    return users;
}

void HealthManager::update_user(const UserProfile& user) {
    std::scoped_lock const lock(m_mutex);
    m_database.update_user(user);
}

void HealthManager::recalculate_measurements(int user_id) {
    std::scoped_lock const lock(m_mutex);
    recalculate_measurements_locked(user_id);
}

void HealthManager::recalculate_measurements_locked(int user_id) {
    auto users = get_users_locked();
    auto user_iterator = std::ranges::find_if(
        users,
        [user_id](const UserProfile& user_profile) -> bool { return user_profile.id == user_id; });
    if (user_iterator == users.end()) {
        return;
    }
    const auto& user = *user_iterator;

    auto measurements = m_database.get_measurements(user_id);
    for (const auto& measurement : measurements) {
        auto raw_opt = m_database.get_scale_raw_data(measurement.id);
        if (raw_opt) {
            RawMeasurement raw;
            std::chrono::year_month_day const ymd{
                std::chrono::floor<std::chrono::days>(measurement.date)};
            raw.year = static_cast<int>(ymd.year());
            raw.month = static_cast<unsigned int>(ymd.month());
            raw.day = static_cast<unsigned int>(ymd.day());
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
    std::scoped_lock const lock(m_mutex);
    m_database.delete_user(user_id);
}

auto HealthManager::get_measurements(int user_id) const -> std::vector<Measurement> {
    std::scoped_lock const lock(m_mutex);
    return m_database.get_measurements(user_id);
}

auto HealthManager::get_measurements_range(int user_id, std::chrono::year_month_day start_date,
                                           std::chrono::year_month_day end_date) const
    -> std::vector<Measurement> {
    std::scoped_lock const lock(m_mutex);
    return m_database.get_measurements_range(user_id, start_date, end_date);
}

auto HealthManager::ensure_device_registered() -> int {
    std::string_view const dev_key = m_device.get_device_key();
    auto dev_opt = m_database.get_device_by_key(std::string{dev_key});
    if (dev_opt) {
        return dev_opt->id;
    }

    Device new_dev;
    new_dev.name = m_device.get_device_name();
    new_dev.device_key = std::string{dev_key};
    new_dev.pathway = m_device.get_impedance_pathway();
    return m_database.add_device(new_dev);
}

void HealthManager::start_sync(std::function<void(SyncResult)> callback) {
    int device_id = 0;
    std::vector<UserProfile> existing_users;
    {
        std::scoped_lock const lock(m_mutex);
        device_id = ensure_device_registered();
        existing_users = get_users_locked();
    }

    // Safety: Capture dependencies carefully. Since m_worker is joined in destructor,
    // 'this' is safe as long as the worker thread is running. The lock must not be held while the
    // worker is started or joined, otherwise a callback that is already waiting for the lock would
    // deadlock against the joining thread.
    auto wrapped_callback =
        [this, device_id, callback_moved = std::move(callback)](const SyncResult& result) -> void {
        if (result.success) {
            std::scoped_lock const lock(m_mutex);
            process_sync_results(device_id, result);
        }

        if (callback_moved) {
            callback_moved(result);
        }
    };

    m_worker->start_sync(std::move(existing_users), std::move(wrapped_callback));
}

void HealthManager::process_sync_results(int device_id, const SyncResult& result) {
    // 1. Reconcile users discovered on the scale. A profile is only linked automatically when
    // its name matches exactly one local profile; ambiguous or unknown names are left for the
    // user to reconcile through associate_scale_user().
    auto all_existing_users = m_database.get_users();

    for (const auto& user : result.new_users) {
        std::vector<int> matching_user_ids;
        for (const auto& local_user : all_existing_users) {
            if (local_user.name == user.name) {
                matching_user_ids.push_back(local_user.id);
            }
        }
        if (matching_user_ids.size() != 1) {
            continue;
        }

        int const user_id = matching_user_ids.front();
        const auto existing_mappings = m_database.get_device_user_mappings(user_id);
        bool const already_mapped = std::ranges::any_of(
            existing_mappings, [&](const DeviceUserMapping& device_mapping) -> bool {
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

    // 2. Map scale user IDs to database user IDs and save measurements
    auto all_users = get_users_locked();
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
    std::scoped_lock const lock(m_mutex);
    const int device_id = ensure_device_registered();
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

auto HealthManager::get_unmapped_scale_users(const SyncResult& result) const
    -> std::vector<UserProfile> {
    std::scoped_lock const lock(m_mutex);
    auto const local_users = get_users_locked();
    std::vector<UserProfile> unmapped_users;
    for (const auto& scale_user : result.new_users) {
        bool const is_mapped =
            std::ranges::any_of(local_users, [&scale_user](const UserProfile& local_user) -> bool {
                return !local_user.device_user_id.empty() &&
                       local_user.device_user_id == scale_user.device_user_id;
            });
        if (!is_mapped) {
            unmapped_users.push_back(scale_user);
        }
    }
    return unmapped_users;
}

void HealthManager::stop_sync() {
    m_worker->stop();
}

auto HealthManager::get_devices() const -> std::vector<Device> {
    std::scoped_lock const lock(m_mutex);
    return m_database.get_devices();
}

void HealthManager::update_device(const Device& device) {
    std::scoped_lock const lock(m_mutex);
    m_database.update_device(device);
}

void HealthManager::recalculate_device_measurements(int device_id) {
    std::scoped_lock const lock(m_mutex);
    recalculate_device_measurements_locked(device_id);
}

void HealthManager::recalculate_device_measurements_locked(int device_id) {
    auto users = get_users_locked();
    for (const auto& user : users) {
        auto measurements = m_database.get_measurements(user.id);
        bool const has_device_measurement =
            std::ranges::any_of(measurements, [device_id](const Measurement& measurement) -> bool {
                return measurement.device_id && *measurement.device_id == device_id;
            });
        if (has_device_measurement) {
            recalculate_measurements_locked(user.id);
        }
    }
}

}  // namespace libscale
