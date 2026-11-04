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
     * @return The database ID of the inserted user.
     */
    auto add_user(const UserProfile& user) -> int override;

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
     * @return The effective stored timestamp, which may differ from the requested date when a
     * collision was resolved or an identical record was skipped.
     */
    auto add_measurement(const Measurement& measurement) -> std::chrono::sys_seconds override;

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

    /**
     * @brief Fetches all registered physical scale devices.
     * @return List of devices.
     */
    auto get_devices() const -> std::vector<Device> override;

    /**
     * @brief Inserts a new scale device record.
     * @param device The device to insert.
     * @return The database ID of the inserted device.
     */
    auto add_device(const Device& device) -> int override;

    /**
     * @brief Updates an existing scale device record in the SQLite database.
     * @param device The device struct with updated fields.
     */
    void update_device(const Device& device) override;

    /**
     * @brief Fetches a scale device by its database ID.
     * @param device_id The database ID of the device.
     * @return The device record if found, std::nullopt otherwise.
     */
    auto get_device(int device_id) const -> std::optional<Device> override;

    /**
     * @brief Fetches a scale device by its hardware key.
     * @param device_key The hardware key (e.g. "usb:222d:0001").
     * @return The device record if found, std::nullopt otherwise.
     */
    auto get_device_by_key(const std::string& device_key) const -> std::optional<Device> override;

    /**
     * @brief Fetches all device-user mappings for a specific software user profile.
     * @param user_id The database ID of the user.
     * @return List of device-user mappings.
     */
    auto get_device_user_mappings(int user_id) const -> std::vector<DeviceUserMapping> override;

    /**
     * @brief Fetches all device-user mappings registered for a specific physical device.
     * @param device_id Database ID of the device.
     * @return List of device-user mappings for the device.
     */
    auto get_device_user_mappings_by_device(int device_id) const
        -> std::vector<DeviceUserMapping> override;

    /**
     * @brief Inserts a new device-user mapping.
     * @param mapping The mapping to insert.
     * @return The database ID of the inserted mapping.
     */
    auto add_device_user_mapping(const DeviceUserMapping& mapping) -> int override;

    /**
     * @brief Fetches a device-user mapping by device ID and scale-specific user ID string.
     * @param device_id The database ID of the device.
     * @param device_user_id The scale-specific user ID identifier string.
     * @return The mapping record if found, std::nullopt otherwise.
     */
    auto get_device_user_mapping(int device_id, const std::string& device_user_id) const
        -> std::optional<DeviceUserMapping> override;

   private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    mutable std::shared_mutex m_mutex;
};

}  // namespace libscale
