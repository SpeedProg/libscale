#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "libscale/database_impl.hpp"
#include "libscale/device.hpp"
#include "libscale/manager.hpp"

namespace libscale {

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
    HealthManager manager(database, device);
    std::cout << "HealthManager initialized.\n";

    // 4. Test User Creation
    manager.add_user("Test User");
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
