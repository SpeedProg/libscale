#pragma once
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <vector>

#include "libscale/database.hpp"

namespace libscale {

/**
 * @brief Concrete database manager implementing SQLite storage and retrieval via SQLite-ORM.
 * Uses a std::shared_mutex to safely coordinate concurrent reads and writes across threads.
 */
class SqliteDatabaseManager : public IDatabaseManager {
   public:
    /**
     * @brief Constructs a new SqliteDatabaseManager.
     */
    SqliteDatabaseManager();

    /**
     * @brief Destructor. Closes SQLite connections and releases internal PIMPL implementation
     * resources.
     */
    ~SqliteDatabaseManager() override;

    SqliteDatabaseManager(const SqliteDatabaseManager&) = delete;
    auto operator=(const SqliteDatabaseManager&) -> SqliteDatabaseManager& = delete;
    SqliteDatabaseManager(SqliteDatabaseManager&&) = delete;
    auto operator=(SqliteDatabaseManager&&) -> SqliteDatabaseManager& = delete;

    /**
     * @brief Initializes the database connection and runs schemas/migrations if required.
     * @param db_path Path to the SQLite database file on disk.
     * @return True if initialized successfully, false on filesystem or initialization error.
     */
    auto initialize(const std::filesystem::path& db_path) -> bool override;

    /**
     * @brief Fetches all registered user profiles from the local SQLite storage.
     * @return List of user profiles.
     */
    auto get_users() const -> std::vector<UserProfile> override;

    /**
     * @brief Inserts a new user profile record into the SQLite database.
     * @param user The user profile data to store.
     */
    void add_user(const UserProfile& user) override;

    /**
     * @brief Updates biometric details of an existing user profile in the SQLite database.
     * @param user The updated user profile data.
     */
    void update_user(const UserProfile& user) override;

    /**
     * @brief Deletes a user profile and all nested measurement records from SQLite.
     * @param user_id Database ID of the user.
     */
    void delete_user(int user_id) override;

    /**
     * @brief Retrieves all historical measurements for a specific user.
     * @param user_id Database ID of the user.
     * @return List of processed measurements, sorted chronologically.
     */
    auto get_measurements(int user_id) const -> std::vector<Measurement> override;

    /**
     * @brief Retrieves historical measurements for a specific user within a date range.
     * @param user_id Database ID of the user.
     * @param start Beginning date of the range (inclusive).
     * @param end Ending date of the range (inclusive).
     * @return List of processed measurements matching the query range.
     */
    auto get_measurements_range(int user_id, std::chrono::year_month_day start,
                                std::chrono::year_month_day end) const
        -> std::vector<Measurement> override;

    /**
     * @brief Inserts a new measurement record into the SQLite database.
     * @param measurement Computed measurement record to store.
     */
    void add_measurement(const Measurement& measurement) override;

    /**
     * @brief Inserts raw Comfort Select biometric impedance data linked to a measurement record.
     * @param raw_data The raw Comfort Select impedance data record.
     */
    void add_scale_raw_data(const ScaleRawData& raw_data) override;

    /**
     * @brief Fetches raw Comfort Select biometric impedance data for a specific measurement ID.
     * @param measurement_id The database ID of the parent measurement.
     * @return Raw impedance record if found, std::nullopt otherwise.
     */
    auto get_scale_raw_data(int measurement_id) const -> std::optional<ScaleRawData> override;

    /**
     * @brief Deletes a single measurement record from SQLite by database ID.
     * @param measurement_id Database ID of the measurement.
     */
    void delete_measurement(int measurement_id) override;

   private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    mutable std::shared_mutex m_mutex;
};

}  // namespace libscale
