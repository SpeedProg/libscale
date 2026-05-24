#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "calculator.hpp"
#include "database.hpp"
#include "device.hpp"
#include "types.hpp"
#include "worker.hpp"

namespace libscale {

/**
 * @brief The central service layer coordinating database access, hardware communication,
 * metric calculations, and background scale synchronization.
 * Follows the Single Responsibility Principle by delegating specific logic to specialized classes.
 */
class HealthManager {
   public:
    /**
     * @brief Constructs a new HealthManager with dependency injection.
     * @param database Reference to the database manager interface.
     * @param device Reference to the scale hardware device interface.
     * @param calculator Unique pointer to the bio-impedance calculator engine (optional).
     */
    HealthManager(IDatabaseManager& database, IScaleDevice& device,
                  std::unique_ptr<IBioImpedanceCalculator> calculator = nullptr);

    /**
     * @brief Destructor. Cleans up background threads and sync workers.
     */
    ~HealthManager();

    HealthManager(const HealthManager&) = delete;
    auto operator=(const HealthManager&) -> HealthManager& = delete;
    HealthManager(HealthManager&&) = delete;
    auto operator=(HealthManager&&) -> HealthManager& = delete;

    /**
     * @brief Retrieves all user profiles registered in the system database.
     * @return List of user profiles.
     */
    [[nodiscard]] auto get_users() const -> std::vector<UserProfile>;

    /**
     * @brief Updates biometric details of an existing user profile in the database.
     * @param user The updated user profile containing valid modified fields.
     */
    void update_user(const UserProfile& user);

    /**
     * @brief Recalculates all historical measurements for a specific user.
     * Useful when user attributes (like height, birthdate, or waist circumference) are updated.
     * @param user_id The unique database ID of the user.
     */
    void recalculate_measurements(int user_id);

    /**
     * @brief Deletes a user profile and all associated measurements from the database.
     * @param user_id The unique database ID of the user.
     */
    void delete_user(int user_id);

    /**
     * @brief Retrieves all historical measurements for a specific user.
     * @param user_id The unique database ID of the user.
     * @return List of computed measurements, sorted chronologically.
     */
    [[nodiscard]] auto get_measurements(int user_id) const -> std::vector<Measurement>;

    /**
     * @brief Retrieves historical measurements for a specific user within a date range.
     * @param user_id The unique database ID of the user.
     * @param start_date Beginning date of the range (inclusive).
     * @param end_date Ending date of the range (inclusive).
     * @return List of computed measurements matching the query range.
     */
    [[nodiscard]] auto get_measurements_range(int user_id, std::chrono::year_month_day start_date,
                                              std::chrono::year_month_day end_date) const
        -> std::vector<Measurement>;

    /**
     * @brief Initiates an asynchronous background synchronization with the scale device.
     * Starts a worker thread to fetch users and measurements.
     * @param callback Callback function executed upon sync completion or failure.
     */
    void start_sync(std::function<void(SyncResult)> callback);

    /**
     * @brief Requests any active background scale synchronization to stop immediately.
     */
    void stop_sync();

    /**
     * @brief Manually associates an unmapped scale user profile with a local database user profile,
     * or creates a new profile if no user ID is provided, and imports their sync measurements.
     * @param scale_user The user profile details from the scale.
     * @param user_id Optional existing local database user ID to link to. If empty, a new user is
     * created.
     * @param result The synchronization result containing the measurements to import.
     */
    void associate_scale_user(const UserProfile& scale_user, std::optional<int> user_id,
                              const SyncResult& result);

   private:
    void process_sync_results(int device_id, const SyncResult& result);

    IDatabaseManager& m_database;
    IScaleDevice& m_device;
    std::unique_ptr<IBioImpedanceCalculator> m_calculator;
    std::unique_ptr<SyncWorker> m_worker;
};

}  // namespace libscale
