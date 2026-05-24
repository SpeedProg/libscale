#include <gtest/gtest.h>
#include <sigc++/signal.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "libscale/calculator.hpp"
#include "libscale/database.hpp"
#include "libscale/database_impl.hpp"
#include "libscale/device.hpp"
#include "libscale/manager.hpp"
#include "libscale/types.hpp"
#include "libscale/worker.hpp"

using namespace libscale;

namespace {

// NOLINTBEGIN(misc-non-private-member-variables-in-classes): Mock scale and database helper classes
// for testing
/**
 * @brief An in-memory fake implementation of IDatabaseManager for testing.
 */
class FakeDatabaseManager : public IDatabaseManager {
   public:
    /// List of in-memory user profiles.
    std::vector<UserProfile> users;
    /// List of in-memory devices.
    std::vector<Device> devices;
    /// List of in-memory device-user mappings.
    std::vector<DeviceUserMapping> mappings;
    /// List of in-memory measurements.
    std::vector<Measurement> measurements;
    /// List of in-memory raw scale data.
    std::vector<ScaleRawData> raw_datas;

    /**
     * @brief Constructor.
     */
    FakeDatabaseManager() = default;

    /**
     * @brief Destructor.
     */
    ~FakeDatabaseManager() override = default;

    FakeDatabaseManager(const FakeDatabaseManager&) = delete;
    auto operator=(const FakeDatabaseManager&) -> FakeDatabaseManager& = delete;
    FakeDatabaseManager(FakeDatabaseManager&&) = delete;
    auto operator=(FakeDatabaseManager&&) -> FakeDatabaseManager& = delete;

    /**
     * @brief Fake initialization.
     * @param db_path Path to the database.
     * @return Always true.
     */
    auto initialize(const std::filesystem::path& db_path) -> bool override {
        (void)db_path;
        return true;
    }

    /**
     * @brief Gets all user profiles.
     * @return List of user profiles.
     */
    [[nodiscard]] auto get_users() const -> std::vector<UserProfile> override {
        return users;
    }

    /**
     * @brief Adds a new user profile.
     * @param user The user profile to add.
     * @return The auto-incremented ID of the user.
     */
    auto add_user(const UserProfile& user) -> int override {
        UserProfile user_copy = user;
        user_copy.id = static_cast<int>(users.size() + 1);
        users.push_back(user_copy);
        return user_copy.id;
    }

    /**
     * @brief Updates an existing user profile.
     * @param user The user profile to update.
     */
    void update_user(const UserProfile& user) override {
        for (auto& existing_user : users) {
            if (existing_user.id == user.id) {
                existing_user = user;
                break;
            }
        }
    }

    /**
     * @brief Deletes a user profile by ID.
     * @param user_id The ID of the user to delete.
     */
    void delete_user(int user_id) override {
        std::erase_if(users, [user_id](const auto& user) { return user.id == user_id; });
    }

    /**
     * @brief Gets all measurements for a user.
     * @param user_id The ID of the user.
     * @return List of measurements.
     */
    [[nodiscard]] auto get_measurements(int user_id) const -> std::vector<Measurement> override {
        std::vector<Measurement> result;
        for (const auto& measurement : measurements) {
            if (measurement.user_id == user_id) {
                result.push_back(measurement);
            }
        }
        return result;
    }

    /**
     * @brief Gets all measurements for a user within a range.
     * @param user_id The ID of the user.
     * @param start Start of the range.
     * @param end End of the range.
     * @return List of measurements.
     */
    [[nodiscard]] auto get_measurements_range(int user_id, std::chrono::year_month_day start,
                                              std::chrono::year_month_day end) const
        -> std::vector<Measurement> override {
        (void)start;
        (void)end;
        return get_measurements(user_id);
    }

    /**
     * @brief Adds a new measurement.
     * @param measurement The measurement to add.
     */
    void add_measurement(const Measurement& measurement) override {
        measurements.push_back(measurement);
    }

    /**
     * @brief Adds scale raw data.
     * @param raw_data The raw data.
     */
    void add_scale_raw_data(const ScaleRawData& raw_data) override {
        raw_datas.push_back(raw_data);
    }

    /**
     * @brief Gets scale raw data.
     * @param measurement_id The measurement ID.
     * @return Optional raw data if found, std::nullopt otherwise.
     */
    [[nodiscard]] auto get_scale_raw_data(int measurement_id) const
        -> std::optional<ScaleRawData> override {
        for (const auto& raw : raw_datas) {
            if (raw.measurement_id == measurement_id) {
                return raw;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Deletes a measurement.
     * @param measurement_id The measurement ID to delete.
     */
    void delete_measurement(int measurement_id) override {
        (void)measurement_id;
    }

    /**
     * @brief Gets all devices.
     * @return List of devices.
     */
    [[nodiscard]] auto get_devices() const -> std::vector<Device> override {
        return devices;
    }

    /**
     * @brief Adds a new device.
     * @param device The device to add.
     * @return The auto-incremented ID of the device.
     */
    auto add_device(const Device& device) -> int override {
        Device device_copy = device;
        device_copy.id = static_cast<int>(devices.size() + 1);
        devices.push_back(device_copy);
        return device_copy.id;
    }

    /**
     * @brief Updates an existing device.
     * @param device The device to update.
     */
    void update_device(const Device& device) override {
        for (auto& existing_device : devices) {
            if (existing_device.id == device.id) {
                existing_device = device;
                break;
            }
        }
    }

    /**
     * @brief Gets a device by ID.
     * @param device_id The ID of the device.
     * @return Optional device.
     */
    [[nodiscard]] auto get_device(int device_id) const -> std::optional<Device> override {
        for (const auto& device : devices) {
            if (device.id == device_id) {
                return device;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Gets a device by key.
     * @param device_key The key of the device.
     * @return Optional device.
     */
    [[nodiscard]] auto get_device_by_key(const std::string& device_key) const
        -> std::optional<Device> override {
        for (const auto& device : devices) {
            if (device.device_key == device_key) {
                return device;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Gets device-user mappings for a user.
     * @param user_id The ID of the user.
     * @return List of mappings.
     */
    [[nodiscard]] auto get_device_user_mappings(int user_id) const
        -> std::vector<DeviceUserMapping> override {
        std::vector<DeviceUserMapping> result;
        for (const auto& mapping : mappings) {
            if (mapping.user_id == user_id) {
                result.push_back(mapping);
            }
        }
        return result;
    }

    /**
     * @brief Adds a new device-user mapping.
     * @param mapping The mapping to add.
     * @return The auto-incremented ID of the mapping.
     */
    auto add_device_user_mapping(const DeviceUserMapping& mapping) -> int override {
        DeviceUserMapping mapping_copy = mapping;
        mapping_copy.id = static_cast<int>(mappings.size() + 1);
        mappings.push_back(mapping_copy);
        return mapping_copy.id;
    }

    /**
     * @brief Gets a mapping.
     * @param device_id The ID of the device.
     * @param device_user_id The scale-specific user ID.
     * @return Optional mapping.
     */
    [[nodiscard]] auto get_device_user_mapping(int device_id,
                                               const std::string& device_user_id) const
        -> std::optional<DeviceUserMapping> override {
        for (const auto& mapping : mappings) {
            if (mapping.device_id == device_id && mapping.device_user_id == device_user_id) {
                return mapping;
            }
        }
        return std::nullopt;
    }
};

class MockScaleDevice : public IScaleDevice {
   public:
    bool connect_result = true;
    bool is_connected_val = true;
    std::vector<UserProfile> users_to_return;
    std::vector<RawMeasurement> measurements_to_return;
    sigc::signal<void()> device_arrived_signal;

    auto connect() -> bool override {
        return connect_result;
    }
    void disconnect() override {}
    auto is_connected() const -> bool override {
        return is_connected_val;
    }
    auto read_users() -> std::vector<UserProfile> override {
        return users_to_return;
    }
    auto read_measurements(const std::string& /*device_user_id*/)
        -> std::vector<RawMeasurement> override {
        return measurements_to_return;
    }
    auto signal_device_arrived() -> sigc::signal<void()> override {
        return device_arrived_signal;
    }
    auto get_device_key() const -> std::string override {
        return "usb:222d:0001";
    }
    auto get_device_name() const -> std::string override {
        return "Mock Soehnle Scale";
    }
    auto get_impedance_pathway() const -> ImpedancePathway override {
        return ImpedancePathway::FootToFoot;
    }
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

}  // namespace

TEST(SyncValidationTest, FullSyncSuccessWhenNamesMatch) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    MockScaleDevice device;
    BioImpedanceCalculatorV1 const calculator;
    SyncWorker worker(device, calculator);

    UserProfile local_user;
    local_user.id = 1;
    local_user.name = "Alice";
    local_user.device_user_id = "slot_1";
    local_user.slot_id = 1;

    std::vector<UserProfile> const existing_users = {local_user};

    UserProfile scale_user;
    scale_user.name = "Alice";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;

    device.users_to_return = {scale_user};

    worker.start_sync(existing_users, [&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_TRUE(sync_outcome.success);
    EXPECT_TRUE(sync_outcome.error_message.empty());
}

TEST(SyncValidationTest, FullSyncFailsWhenNamesMismatch) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    MockScaleDevice device;
    BioImpedanceCalculatorV1 const calculator;
    SyncWorker worker(device, calculator);

    UserProfile local_user;
    local_user.id = 1;
    local_user.name = "Alice";
    local_user.device_user_id = "slot_1";
    local_user.slot_id = 1;

    std::vector<UserProfile> const existing_users = {local_user};

    UserProfile scale_user;
    scale_user.name = "Charlie";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;

    device.users_to_return = {scale_user};

    worker.start_sync(existing_users, [&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_FALSE(sync_outcome.success);
    EXPECT_EQ(sync_outcome.error_message,
              "The users on this scale do not match the expected profiles. Please "
              "re-configure your device.");
}

TEST(SyncValidationTest, UserSyncSuccessWhenNameMatches) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    MockScaleDevice device;
    BioImpedanceCalculatorV1 const calculator;
    SyncWorker worker(device, calculator);

    UserProfile local_user;
    local_user.id = 1;
    local_user.name = "Alice";
    local_user.device_user_id = "slot_1";
    local_user.slot_id = 1;

    UserProfile scale_user;
    scale_user.name = "Alice";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;

    device.users_to_return = {scale_user};

    worker.start_sync_user(local_user, [&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_TRUE(sync_outcome.success);
    EXPECT_TRUE(sync_outcome.error_message.empty());
}

TEST(SyncValidationTest, UserSyncFailsWhenNameMismatches) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    MockScaleDevice device;
    BioImpedanceCalculatorV1 const calculator;
    SyncWorker worker(device, calculator);

    UserProfile local_user;
    local_user.id = 1;
    local_user.name = "Alice";
    local_user.device_user_id = "slot_1";
    local_user.slot_id = 1;

    UserProfile scale_user;
    scale_user.name = "Charlie";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;

    device.users_to_return = {scale_user};

    worker.start_sync_user(local_user, [&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_FALSE(sync_outcome.success);
    EXPECT_EQ(sync_outcome.error_message,
              "The users on this scale do not match the expected profiles. Please "
              "re-configure your device.");
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
TEST(HealthManagerSyncTest, SyncWithEmptyDatabase) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    FakeDatabaseManager database;
    MockScaleDevice device;

    UserProfile scale_user;
    scale_user.name = "Alice";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;
    scale_user.height = units::Cm{170};
    scale_user.gender = Gender::Female;
    scale_user.activity_level = ActivityLevel::HomeGarden;
    scale_user.birthday = std::chrono::year(1990) / 1 / 1;

    device.users_to_return = {scale_user};

    RawMeasurement raw;
    raw.year = 2026;
    raw.month = 5;
    raw.day = 22;
    raw.weight_raw = 650;
    raw.imp5k = 500;
    raw.imp50k = 450;
    device.measurements_to_return = {raw};

    HealthManager manager(database, device);

    manager.start_sync([&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_TRUE(sync_outcome.success);

    // Simulate frontend manually linking the unmapped user to a new profile
    for (const auto& scale_user_profile : sync_outcome.new_users) {
        auto all_local = manager.get_users();
        bool const is_mapped = std::ranges::any_of(all_local, [&](const UserProfile& local) {
            return local.device_user_id == scale_user_profile.device_user_id;
        });
        if (!is_mapped) {
            manager.associate_scale_user(scale_user_profile, std::nullopt, sync_outcome);
        }
    }

    auto users = database.get_users();
    ASSERT_EQ(users.size(), 1);
    EXPECT_EQ(users[0].name, "Alice");

    auto mappings = database.get_device_user_mappings(users[0].id);
    ASSERT_EQ(mappings.size(), 1);
    EXPECT_EQ(mappings[0].device_user_id, "slot_1");

    auto measurements = database.get_measurements(users[0].id);
    ASSERT_EQ(measurements.size(), 1);
    EXPECT_NEAR(measurements[0].weight.value, 65.0, 0.001);
}

TEST(HealthManagerSyncTest, FirstSyncMapsByName) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    FakeDatabaseManager database;
    MockScaleDevice device;

    UserProfile local_user;
    local_user.name = "Alice";
    local_user.height = units::Cm{170};
    local_user.gender = Gender::Female;
    local_user.activity_level = ActivityLevel::HomeGarden;
    local_user.birthday = std::chrono::year(1990) / 1 / 1;
    int const local_user_id = database.add_user(local_user);

    UserProfile scale_user;
    scale_user.name = "Alice";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;
    scale_user.height = units::Cm{170};
    scale_user.gender = Gender::Female;
    scale_user.activity_level = ActivityLevel::HomeGarden;
    scale_user.birthday = std::chrono::year(1990) / 1 / 1;

    device.users_to_return = {scale_user};

    RawMeasurement raw;
    raw.year = 2026;
    raw.month = 5;
    raw.day = 22;
    raw.weight_raw = 650;
    raw.imp5k = 500;
    raw.imp50k = 450;
    device.measurements_to_return = {raw};

    HealthManager manager(database, device);

    manager.start_sync([&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_TRUE(sync_outcome.success);

    auto users = database.get_users();
    ASSERT_EQ(users.size(), 1);
    EXPECT_EQ(users[0].id, local_user_id);
    EXPECT_EQ(users[0].name, "Alice");

    auto mappings = database.get_device_user_mappings(local_user_id);
    ASSERT_EQ(mappings.size(), 1);
    EXPECT_EQ(mappings[0].device_user_id, "slot_1");

    auto measurements = database.get_measurements(local_user_id);
    ASSERT_EQ(measurements.size(), 1);
    EXPECT_NEAR(measurements[0].weight.value, 65.0, 0.001);
}

TEST(HealthManagerSyncTest, ManualAssociation) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    FakeDatabaseManager database;
    MockScaleDevice device;

    UserProfile local_user;
    local_user.name = "Bob";
    local_user.height = units::Cm{180};
    local_user.gender = Gender::Male;
    local_user.activity_level = ActivityLevel::Light;
    local_user.birthday = std::chrono::year(1985) / 5 / 15;
    int const local_user_id = database.add_user(local_user);

    UserProfile scale_user;
    scale_user.name = "Alice";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;
    scale_user.height = units::Cm{170};
    scale_user.gender = Gender::Female;
    scale_user.activity_level = ActivityLevel::HomeGarden;
    scale_user.birthday = std::chrono::year(1990) / 1 / 1;

    device.users_to_return = {scale_user};

    RawMeasurement raw;
    raw.year = 2026;
    raw.month = 5;
    raw.day = 22;
    raw.weight_raw = 650;
    raw.imp5k = 500;
    raw.imp50k = 450;
    device.measurements_to_return = {raw};

    HealthManager manager(database, device);

    manager.start_sync([&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_TRUE(sync_outcome.success);

    // Verify Bob is not mapped initially
    auto users_before = database.get_users();
    ASSERT_EQ(users_before.size(), 1);
    EXPECT_EQ(users_before[0].name, "Bob");

    // Link Alice to Bob manually
    manager.associate_scale_user(scale_user, local_user_id, sync_outcome);

    // Verify Bob is now mapped to Alice's scale ID and has the measurements
    auto manager_users = manager.get_users();
    ASSERT_EQ(manager_users.size(), 1);
    EXPECT_EQ(manager_users[0].device_user_id, "slot_1");

    auto measurements = database.get_measurements(local_user_id);
    ASSERT_EQ(measurements.size(), 1);
    EXPECT_NEAR(measurements[0].weight.value, 65.0, 0.001);
}

TEST(HealthManagerSyncTest, RealDbSyncWithEmptyDatabase) {
    std::mutex mutex_var;
    std::condition_variable cv_var;
    bool callback_called = false;
    SyncResult sync_outcome;

    std::filesystem::path const db_path = "test_empty_sync.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    SqliteDatabaseManager database;
    ASSERT_TRUE(database.initialize(db_path));

    MockScaleDevice device;

    UserProfile scale_user;
    scale_user.name = "Alice";
    scale_user.device_user_id = "slot_1";
    scale_user.slot_id = 1;
    scale_user.height = units::Cm{170};
    scale_user.gender = Gender::Female;
    scale_user.activity_level = ActivityLevel::HomeGarden;
    scale_user.birthday = std::chrono::year(1990) / 1 / 1;

    device.users_to_return = {scale_user};

    RawMeasurement raw;
    raw.year = 2026;
    raw.month = 5;
    raw.day = 22;
    raw.weight_raw = 650;
    raw.imp5k = 500;
    raw.imp50k = 450;
    device.measurements_to_return = {raw};

    HealthManager manager(database, device);

    manager.start_sync([&](SyncResult result) {
        std::lock_guard<std::mutex> const lock(mutex_var);
        sync_outcome = std::move(result);
        callback_called = true;
        cv_var.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex_var);
    static constexpr int wait_seconds = 2;
    cv_var.wait_for(lock, std::chrono::seconds(wait_seconds), [&] { return callback_called; });

    ASSERT_TRUE(callback_called);
    EXPECT_TRUE(sync_outcome.success);

    // Simulate frontend manually linking the unmapped user to a new profile
    for (const auto& scale_user_profile : sync_outcome.new_users) {
        auto all_local = manager.get_users();
        bool const is_mapped = std::ranges::any_of(all_local, [&](const UserProfile& local) {
            return local.device_user_id == scale_user_profile.device_user_id;
        });
        if (!is_mapped) {
            manager.associate_scale_user(scale_user_profile, std::nullopt, sync_outcome);
        }
    }

    auto users = database.get_users();
    ASSERT_EQ(users.size(), 1);
    EXPECT_EQ(users[0].name, "Alice");

    auto mappings = database.get_device_user_mappings(users[0].id);
    ASSERT_EQ(mappings.size(), 1);
    EXPECT_EQ(mappings[0].device_user_id, "slot_1");

    auto measurements = database.get_measurements(users[0].id);
    ASSERT_EQ(measurements.size(), 1);
    EXPECT_NEAR(measurements[0].weight.value, 65.0, 0.001);

    // Clean up
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}

TEST(HealthManagerDeviceTest, GetAndUpdateDeviceAndRecalculate) {
    FakeDatabaseManager database;
    MockScaleDevice device;

    // 1. Add device
    Device dev;
    dev.id = 1;
    dev.name = "Initial Soehnle";
    dev.device_key = "usb:222d:0001";
    dev.pathway = ImpedancePathway::FootToFoot;
    database.devices.push_back(dev);

    HealthManager manager(database, device);

    // Verify get_devices
    auto list = manager.get_devices();
    ASSERT_EQ(list.size(), 1);
    EXPECT_EQ(list[0].name, "Initial Soehnle");

    // Verify update_device
    dev.name = "Updated Soehnle";
    dev.pathway = ImpedancePathway::HandToFoot;
    manager.update_device(dev);

    auto list_updated = manager.get_devices();
    ASSERT_EQ(list_updated.size(), 1);
    EXPECT_EQ(list_updated[0].name, "Updated Soehnle");
    EXPECT_EQ(list_updated[0].pathway, ImpedancePathway::HandToFoot);

    // 2. Set up user and measurement to test recalculate_device_measurements
    UserProfile user;
    user.id = 1;
    user.name = "Alice";
    user.height = units::Cm{170};
    user.gender = Gender::Female;
    user.activity_level = ActivityLevel::HomeGarden;
    user.birthday = std::chrono::year(1990) / 1 / 1;
    database.users.push_back(user);

    // Measurement with device_id = 1
    Measurement meas;
    meas.id = 10;
    meas.user_id = 1;
    meas.device_id = 1;
    meas.date = std::chrono::year(2026) / 5 / 22;
    meas.weight = units::Kg{65.0};
    meas.body_fat = units::Percent{25.0};
    meas.muscle_mass = units::Percent{45.0};
    meas.water = units::Percent{50.0};
    database.measurements.push_back(meas);

    // Raw scale data
    ScaleRawData raw_data;
    raw_data.measurement_id = 10;
    raw_data.weight_raw = 650;
    raw_data.imp5k = 500;
    raw_data.imp50k = 450;
    database.raw_datas.push_back(raw_data);

    // Perform recalculation
    manager.recalculate_device_measurements(1);

    // Verify recalculation was executed and appended a new measurement
    ASSERT_GT(database.measurements.size(), 1);
    const auto& recalculated = database.measurements.back();
    EXPECT_EQ(recalculated.id, 10);
    EXPECT_EQ(recalculated.user_id, 1);
    ASSERT_TRUE(recalculated.device_id.has_value());
    if (recalculated.device_id) {
        EXPECT_EQ(*recalculated.device_id, 1);
    }
    EXPECT_NEAR(recalculated.weight.value, 65.0, 0.001);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
