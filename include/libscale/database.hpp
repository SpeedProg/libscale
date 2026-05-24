#pragma once
#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace libscale {

/**
 * @brief Abstract interface defining operations for data persistence.
 * Decouples database technology implementations from high-level management layers.
 */
class IDatabaseManager {
   public:
    /**
     * @brief Default constructor.
     */
    IDatabaseManager() = default;

    /**
     * @brief Virtual destructor to ensure proper cleanup of derived database connections.
     */
    virtual ~IDatabaseManager() = default;

    IDatabaseManager(const IDatabaseManager&) = delete;
    auto operator=(const IDatabaseManager&) -> IDatabaseManager& = delete;
    IDatabaseManager(IDatabaseManager&&) = delete;
    auto operator=(IDatabaseManager&&) -> IDatabaseManager& = delete;

    /**
     * @brief Initializes the database connection and runs any pending schema migrations.
     * @param db_path Path to the database file on disk.
     * @return True if initialized successfully, false otherwise.
     */
    virtual auto initialize(const std::filesystem::path& db_path) -> bool = 0;

    /**
     * @brief Fetches all registered user profiles from the database.
     * @return List of user profiles.
     */
    [[nodiscard]] virtual auto get_users() const -> std::vector<UserProfile> = 0;

    /**
     * @brief Inserts a new user profile record into the database.
     * @param user The user profile struct populated with data.
     * @return The database ID of the inserted user.
     */
    virtual auto add_user(const UserProfile& user) -> int = 0;

    /**
     * @brief Updates an existing user profile record in the database.
     * @param user The user profile struct with updated fields matching an existing ID.
     */
    virtual void update_user(const UserProfile& user) = 0;

    /**
     * @brief Deletes a user profile and all their measurements from the database.
     * @param user_id The database ID of the user to delete.
     */
    virtual void delete_user(int user_id) = 0;

    /**
     * @brief Retrieves all historical measurements for a specific user from the database.
     * @param user_id The database ID of the user.
     * @return List of measurements sorted chronologically.
     */
    [[nodiscard]] virtual auto get_measurements(int user_id) const -> std::vector<Measurement> = 0;

    /**
     * @brief Retrieves measurements for a user filtered by a specific start/end date range.
     * @param user_id The database ID of the user.
     * @param start Start date of the range (inclusive).
     * @param end End date of the range (inclusive).
     * @return List of matching computed measurements.
     */
    [[nodiscard]] virtual auto get_measurements_range(int user_id,
                                                      std::chrono::year_month_day start,
                                                      std::chrono::year_month_day end) const
        -> std::vector<Measurement> = 0;

    /**
     * @brief Inserts a new measurement record into the database.
     * @param measurement The computed measurement struct.
     */
    virtual void add_measurement(const Measurement& measurement) = 0;

    /**
     * @brief Inserts raw Comfort Select biometric impedance data linked to a measurement record.
     * @param raw_data The raw Comfort Select impedance data record.
     */
    virtual void add_scale_raw_data(const ScaleRawData& raw_data) = 0;

    /**
     * @brief Fetches raw Comfort Select biometric impedance data for a specific measurement ID.
     * @param measurement_id The database ID of the parent measurement.
     * @return Raw impedance record if found, std::nullopt otherwise.
     */
    [[nodiscard]] virtual auto get_scale_raw_data(int measurement_id) const
        -> std::optional<ScaleRawData> = 0;

    /**
     * @brief Deletes a single measurement record by its database ID.
     * @param measurement_id The database ID of the measurement to delete.
     */
    virtual void delete_measurement(int measurement_id) = 0;

    /**
     * @brief Fetches all registered physical scale devices.
     * @return List of devices.
     */
    [[nodiscard]] virtual auto get_devices() const -> std::vector<Device> = 0;

    /**
     * @brief Inserts a new scale device record.
     * @param device The device to insert.
     * @return The database ID of the inserted device.
     */
    virtual auto add_device(const Device& device) -> int = 0;

    /**
     * @brief Updates an existing scale device record in the database.
     * @param device The device struct with updated fields matching an existing ID.
     */
    virtual void update_device(const Device& device) = 0;

    /**
     * @brief Fetches a scale device by its database ID.
     * @param device_id The database ID of the device.
     * @return The device record if found, std::nullopt otherwise.
     */
    [[nodiscard]] virtual auto get_device(int device_id) const -> std::optional<Device> = 0;

    /**
     * @brief Fetches a scale device by its hardware key.
     * @param device_key The hardware key (e.g. "usb:222d:0001").
     * @return The device record if found, std::nullopt otherwise.
     */
    [[nodiscard]] virtual auto get_device_by_key(const std::string& device_key) const
        -> std::optional<Device> = 0;

    /**
     * @brief Fetches all device-user mappings for a specific software user profile.
     * @param user_id The database ID of the user.
     * @return List of device-user mappings.
     */
    [[nodiscard]] virtual auto get_device_user_mappings(int user_id) const
        -> std::vector<DeviceUserMapping> = 0;

    /**
     * @brief Inserts a new device-user mapping.
     * @param mapping The mapping to insert.
     * @return The database ID of the inserted mapping.
     */
    virtual auto add_device_user_mapping(const DeviceUserMapping& mapping) -> int = 0;

    /**
     * @brief Fetches a device-user mapping by device ID and scale-specific user ID string.
     * @param device_id The database ID of the device.
     * @param device_user_id The scale-specific user ID identifier string.
     * @return The mapping record if found, std::nullopt otherwise.
     */
    [[nodiscard]] virtual auto get_device_user_mapping(int device_id,
                                                       const std::string& device_user_id) const
        -> std::optional<DeviceUserMapping> = 0;
};

}  // namespace libscale
