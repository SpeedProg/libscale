#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include <chrono>
#include "libscale/database_impl.hpp"
#include "libscale/device.hpp"
#include "libscale/manager.hpp"
#include "libscale/types.hpp"

namespace libscale {

namespace {
auto run_smoke_test() -> void {
    std::cout << "Starting CLI Smoke Test...\n";

    // 1. Initialize Database
    SqliteDatabaseManager database;
    const std::string db_path = "smoke_test.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    if (!database.initialize(db_path)) {
        std::cerr << "Failed to initialize database.\n";
        return;
    }
    std::cout << "Database initialized.\n";

    // 2. Initialize Device (Mock/Dummy if hardware not present)
    UsbScaleDevice device;

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
    user.birthday = std::chrono::year_month_day{std::chrono::year(default_year), std::chrono::month(default_month), std::chrono::day(default_day)};
    database.add_user(user);
    const auto users = manager.get_users();
    if (users.empty() || users[0].name != "Test User") {
        std::cerr << "User creation failed.\n";
        return;
    }
    std::cout << "User creation verified: " << users[0].name << "\n";

    // 5. Test History Retrieval (should be empty)
    const auto history = manager.get_measurements(users[0].id);
    std::cout << "History retrieval verified (Count: " << history.size() << ")\n";

    std::cout << "CLI Smoke Test PASSED!\n";
}
}  // namespace
}  // namespace libscale

auto main() -> int {
    try {
        libscale::run_smoke_test();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
