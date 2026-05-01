#include "libscale/manager.hpp"

#include <chrono>
#include <functional>
#include <memory>
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

namespace {
constexpr int default_user_height = 175;
constexpr Gender default_gender = Gender::Female;
constexpr ActivityLevel default_activity = ActivityLevel::HomeGarden;
constexpr std::chrono::year_month_day default_birthday{std::chrono::year(1990),
                                                       std::chrono::month(1), std::chrono::day(1)};
}  // namespace

HealthManager::HealthManager(IDatabaseManager& database, IScaleDevice& device,
                             std::unique_ptr<IBioImpedanceCalculator> calculator)
    : m_database(database),
      m_device(device),
      m_calculator(calculator ? std::move(calculator)
                              : std::make_unique<BioImpedanceCalculatorV1>()),
      m_worker(std::make_unique<SyncWorker>(device, *m_calculator)) {}

HealthManager::~HealthManager() = default;

auto HealthManager::get_users() const -> std::vector<UserProfile> {
    return m_database.get_users();
}

void HealthManager::add_user(const std::string& name) {
    if (name.empty()) {
        return;
    }

    auto existing_users = m_database.get_users();

    UserProfile user;
    user.name = name;
    user.slot_id = static_cast<int>(existing_users.size() + 1);
    user.device_user_id = "slot_" + std::to_string(user.slot_id);
    user.height = units::Cm{default_user_height};
    user.gender = default_gender;
    user.activity_level = default_activity;
    user.birthday = default_birthday;

    m_database.add_user(user);
}

void HealthManager::update_user(const UserProfile& user) {
    m_database.update_user(user);
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
    auto existing_users = m_database.get_users();

    // Safety: Capture dependencies carefully. Since m_worker is joined in destructor,
    // 'this' is safe as long as the worker thread is running.
    auto wrapped_callback = [this, callback_moved = std::move(callback)](SyncResult result) {
        if (result.success) {
            // 1. Add new users discovered on scale
            for (const auto& user : result.new_users) {
                m_database.add_user(user);
            }

            // 2. Map scale user IDs to database user IDs and save measurements
            auto all_users = m_database.get_users();
            std::unordered_map<std::string, int> device_id_to_db_id;
            for (const auto& user : all_users) {
                device_id_to_db_id[user.device_user_id] = user.id;
            }

            for (auto& measurement : result.measurements) {
                auto it_user = device_id_to_db_id.find(measurement.device_user_id);
                if (it_user != device_id_to_db_id.end()) {
                    measurement.user_id = it_user->second;
                    // Note: If IDatabaseManager supported replace(), we'd use it here.
                    // For now, we'll keep add_measurement, but with the unique constraint
                    // in database_impl.cpp, we should use REPLACE in the implementation.
                    m_database.add_measurement(measurement);
                }
            }
        }

        if (callback_moved) {
            callback_moved(result);
        }
    };

    m_worker->start_sync(std::move(existing_users), std::move(wrapped_callback));
}

void HealthManager::stop_sync() {
    m_worker->stop();
}

}  // namespace libscale
