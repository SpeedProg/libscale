#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "libscale/database_impl.hpp"
#include "libscale/device.hpp"
#include "libscale/manager.hpp"
#include "libscale/types.hpp"

namespace libscale {

namespace {
auto smoke_test_db_path() -> const std::filesystem::path& {
    static const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "libscale_smoke_test.db";
    return path;
}

auto smoke_test_backup_path() -> std::filesystem::path {
    auto backup = smoke_test_db_path();
    backup += ".tmp_backup";
    return backup;
}

auto run_smoke_test() -> bool {
    std::cout << "Starting CLI Smoke Test...\n";

    // 1. Initialize Database
    SqliteDatabaseManager database;
    auto const& db_path = smoke_test_db_path();
    std::filesystem::remove(db_path);

    if (!database.initialize(db_path)) {
        std::cerr << "Failed to initialize database.\n";
        return false;
    }
    std::cout << "Database initialized.\n";

    // 2. Initialize Device (Mock/Dummy if hardware not present)
    UsbScaleDevice device;

    Device dev;
    dev.name = device.get_device_name();
    dev.device_key = device.get_device_key();
    dev.pathway = device.get_impedance_pathway();
    int const dev_id = database.add_device(dev);

    // 3. Initialize HealthManager
    HealthManager const manager(database, device);
    std::cout << "HealthManager initialized.\n";

    // 4. Test User Creation
    constexpr int test_slot_id = 1;
    constexpr int default_height = 175;
    constexpr int default_year = 1990;
    constexpr unsigned int default_month = 1;
    constexpr unsigned int default_day = 1;

    UserProfile user;
    user.name = "Test User";
    user.slot_id = test_slot_id;
    user.device_user_id = "slot_1";
    user.height = units::Cm{default_height};
    user.gender = Gender::Female;
    user.activity_level = ActivityLevel::HomeGarden;
    user.birthday = std::chrono::year_month_day{std::chrono::year(default_year),
                                                std::chrono::month(default_month),
                                                std::chrono::day(default_day)};
    int const user_id = database.add_user(user);

    DeviceUserMapping mapping;
    mapping.user_id = user_id;
    mapping.device_id = dev_id;
    mapping.slot_id = test_slot_id;
    mapping.device_user_id = "slot_1";
    database.add_device_user_mapping(mapping);

    const auto users = manager.get_users();
    if (users.empty() || users[0].name != "Test User") {
        std::cerr << "User creation failed.\n";
        return false;
    }
    std::cout << "User creation verified: " << users[0].name << "\n";

    // 5. Test History Retrieval (should be empty)
    const auto history = manager.get_measurements(users[0].id);
    std::cout << "History retrieval verified (Count: " << history.size() << ")\n";
    if (!history.empty()) {
        std::cerr << "Expected no measurements for a freshly created user.\n";
        return false;
    }

    std::cout << "CLI Smoke Test PASSED!\n";
    return true;
}
}  // namespace
}  // namespace libscale

auto main() -> int {
    int exit_code = 0;
    try {
        exit_code = libscale::run_smoke_test() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        exit_code = 1;
    }

    std::filesystem::remove(libscale::smoke_test_db_path());
    std::filesystem::remove(libscale::smoke_test_backup_path());
    return exit_code;
}
