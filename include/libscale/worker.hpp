#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "calculator.hpp"
#include "device.hpp"
#include "types.hpp"

namespace libscale {

/**
 * @brief Represents the comprehensive result of a scale synchronization operation.
 */
struct SyncResult {
    /// True if the synchronization finished successfully, false on hardware/connection failure.
    bool success = false;
    /// A list of newly processed biometric measurements retrieved from the hardware.
    std::vector<Measurement> measurements;
    /// A list of newly discovered user profiles configured on the scale.
    std::vector<UserProfile> new_users;
    /// Informative error message populated if the sync operation fails.
    std::string error_message;
};

/**
 * @brief A background worker managing the multithreaded synchronization process with the scale.
 * Encapsulates the execution logic using modern cooperative cancellation (std::jthread)
 * to keep UI/main thread completely responsive.
 *
 * ### Concurrency & Thread-Safety Model
 * 1. **Lifecycle Management (RAII):** The background thread is represented by a `std::jthread`
 * (`m_thread`). Ownership of this worker lies in the `HealthManager`. Upon destruction, the
 * worker's destructor cooperatively requests the background thread to cancel itself (via
 * `std::stop_token`) and blocks until the thread joins, preventing any dangling threads or
 * use-after-free scenarios.
 * 2. **Cooperative Cancellation:** Long-running loops (such as iterating through user records or
 * raw telemetry) periodically inspect `stop_token.stop_requested()`. If cancellation is requested
 * (either because a new sync was initiated or the application is exiting), the worker terminates
 * operations early and cleanly.
 * 3. **Mutually Exclusive Control:** Thread creation and joining are serialized using a mutual
 * exclusion lock
 *    (`m_mutex`), ensuring that only one synchronization task runs at any given time.
 * 4. **UI Thread Boundary Safety:** Background threads must NEVER directly manipulate GTK4 widgets,
 * as GTK is not thread-safe. All results are passed back via thread-safe callbacks. The GUI layer
 * orchestrates these callbacks to safe-bridge data across the thread boundary onto the main GTK
 * loop using `Glib::Dispatcher`.
 */
class SyncWorker {
   public:
    /**
     * @brief Constructs a SyncWorker.
     * @param device Scale communication device.
     * @param calculator Formula engine to process raw impedance values.
     */
    SyncWorker(IScaleDevice& device, const IBioImpedanceCalculator& calculator);

    /**
     * @brief Destructor. Cooperatively cancels and joins background worker threads.
     */
    ~SyncWorker() noexcept;

    SyncWorker(const SyncWorker&) = delete;
    auto operator=(const SyncWorker&) -> SyncWorker& = delete;
    SyncWorker(SyncWorker&&) = delete;
    auto operator=(SyncWorker&&) -> SyncWorker& = delete;

    /**
     * @brief Starts an asynchronous background synchronization of all users and measurements.
     * @param existing_users List of user profiles currently stored in the database.
     * @param callback Callback executed on the background thread upon sync completion.
     */
    void start_sync(std::vector<UserProfile> existing_users,
                    std::function<void(SyncResult)> callback);

    /**
     * @brief Starts an asynchronous background synchronization targeting a single user profile.
     * @param user The specific user profile to synchronize.
     * @param callback Callback executed on the background thread upon sync completion.
     */
    void start_sync_user(UserProfile user, std::function<void(SyncResult)> callback);

    /**
     * @brief Cooperatively requests any active background synchronization threads to stop
     * immediately.
     */
    void stop();

   private:
    void thread_func_full_sync(const std::stop_token& stop_token,
                               std::vector<UserProfile> existing_users,
                               const std::function<void(SyncResult)>& callback);
    void thread_func_user_sync(const std::stop_token& stop_token, const UserProfile& user,
                               const std::function<void(SyncResult)>& callback);

    auto ensure_connected() -> bool;
    void fetch_user_measurements(const std::stop_token& stop_token, const UserProfile& user,
                                 SyncResult& result);
    void execute_sync(const std::stop_token& stop_token,
                      const std::function<void(SyncResult)>& callback,
                      const std::function<void(SyncResult&)>& sync_logic);

    IScaleDevice& m_device;
    const IBioImpedanceCalculator& m_calculator;
    std::jthread m_thread;
    std::mutex m_mutex;
};

}  // namespace libscale
