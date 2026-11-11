#include "libscale/worker.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <utility>
#include <vector>

#include "libscale/calculator.hpp"
#include "libscale/device.hpp"
#include "libscale/types.hpp"

namespace libscale {

SyncWorker::SyncWorker(IScaleDevice& device, const IBioImpedanceCalculator& calculator)
    : m_device(device), m_calculator(calculator) {}

SyncWorker::~SyncWorker() noexcept {
    stop();
}

void SyncWorker::start_sync(std::vector<UserProfile> existing_users, SyncCallback callback) {
    stop();
    std::scoped_lock const lock(m_mutex);
    m_thread = std::jthread(
        [this, users = std::move(existing_users),
         callback_moved = std::move(callback)](const std::stop_token& stop_token) mutable -> void {
            thread_func_full_sync(stop_token, std::move(users), callback_moved);
        });
}

void SyncWorker::start_sync_user(const UserProfile& user, SyncCallback callback) {
    stop();
    std::scoped_lock const lock(m_mutex);
    m_thread = std::jthread([this, user_moved = user, callback_moved = std::move(callback)](
                                const std::stop_token& stop_token) mutable -> void {
        thread_func_user_sync(stop_token, user_moved, callback_moved);
    });
}

void SyncWorker::stop() {
    std::scoped_lock const lock(m_mutex);
    if (m_thread.joinable()) {
        m_thread.request_stop();
        m_thread.join();
    }
}

auto SyncWorker::ensure_connected() -> bool {
    return m_device.is_connected() || m_device.connect();
}

void SyncWorker::fetch_user_measurements(const std::stop_token& stop_token, const UserProfile& user,
                                         SyncResult& result) {
    auto raw_data = m_device.read_measurements(user.device_user_id);
    for (const auto& raw_item : raw_data) {
        if (stop_token.stop_requested()) {
            break;
        }
        auto measurement = m_calculator.calculate(raw_item, user, m_device.get_impedance_pathway());
        if (measurement) {
            result.measurements.push_back(SyncMeasurement{.measurement = *measurement,
                                                          .device_user_id = user.device_user_id});
        }
    }
}

void SyncWorker::execute_sync(const std::stop_token& stop_token, const SyncCallback& callback,
                              const std::function<void(SyncResult&)>& sync_logic) {
    SyncResult result;
    try {
        if (!ensure_connected()) {
            result.error_message = "Could not connect to scale.";
        } else {
            sync_logic(result);
            result.success = !stop_token.stop_requested();
        }
    } catch (const std::exception& error) {
        result.success = false;
        result.error_message = error.what();
    } catch (...) {
        result.success = false;
        result.error_message = "Unknown error during scale synchronization.";
    }

    if (callback && !stop_token.stop_requested()) {
        // The callback runs on the worker thread and may perform database writes. Exceptions must
        // never escape a thread entry point, so failures are logged instead of terminating.
        try {
            callback(result);
        } catch (const std::exception& error) {
            spdlog::error("Unhandled exception in sync completion callback: {}", error.what());
        } catch (...) {
            spdlog::error("Unknown exception in sync completion callback.");
        }
    }
}

void SyncWorker::thread_func_full_sync(const std::stop_token& stop_token,
                                       std::vector<UserProfile> existing_users,
                                       const SyncCallback& callback) {
    execute_sync(stop_token, callback, [&](SyncResult& result) -> void {
        auto scale_users = m_device.read_users();

        // Strict validation: check that mapped profiles match in name
        for (const auto& scale_user : scale_users) {
            auto matching_user_it = std::ranges::find_if(
                existing_users, [&scale_user](const UserProfile& user_profile) -> bool {
                    return !user_profile.device_user_id.empty() &&
                           user_profile.device_user_id == scale_user.device_user_id;
                });
            if (matching_user_it != existing_users.end() &&
                matching_user_it->name != scale_user.name) {
                throw std::runtime_error(
                    "The users on this scale do not match the expected profiles. Please "
                    "re-configure your device.");
            }
        }

        for (const auto& scale_user : scale_users) {
            if (stop_token.stop_requested()) {
                return;
            }
            if (std::ranges::find_if(
                    existing_users, [&scale_user](const UserProfile& user_profile) -> bool {
                        return user_profile.device_user_id == scale_user.device_user_id;
                    }) == existing_users.end()) {
                result.new_users.push_back(scale_user);
            }
        }

        std::vector<UserProfile> all_users = std::move(existing_users);
        all_users.insert(all_users.end(), result.new_users.begin(), result.new_users.end());

        for (const auto& user : all_users) {
            if (stop_token.stop_requested()) {
                return;
            }
            fetch_user_measurements(stop_token, user, result);
        }
    });
}

void SyncWorker::thread_func_user_sync(const std::stop_token& stop_token, const UserProfile& user,
                                       const SyncCallback& callback) {
    execute_sync(stop_token, callback, [&](SyncResult& result) -> void {
        if (!user.device_user_id.empty()) {
            auto scale_users = m_device.read_users();
            auto matching_user_it =
                std::ranges::find_if(scale_users, [&user](const UserProfile& scale_user) -> auto {
                    return scale_user.device_user_id == user.device_user_id;
                });
            if (matching_user_it != scale_users.end() && matching_user_it->name != user.name) {
                throw std::runtime_error(
                    "The users on this scale do not match the expected profiles. Please "
                    "re-configure your device.");
            }
        }
        fetch_user_measurements(stop_token, user, result);
    });
}

}  // namespace libscale
