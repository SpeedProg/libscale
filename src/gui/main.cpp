#include <glibmm/miscutils.h>
#include <gtkmm/application.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "gui/main_window.hpp"
#include "gui/settings.hpp"
#include "libscale/database_impl.hpp"
#include "libscale/device.hpp"
#include "libscale/manager.hpp"
#include "libscale/scale_factory.hpp"

static constexpr int log_max_size = 1024 * 1024 * 5;
static constexpr int log_max_files = 3;
static constexpr int dir_perms = 0700;

auto main(int argc, char* argv[]) -> int {
    // Single-instance behavior is keyed on the application ID, which must match the Flatpak,
    // desktop entry and AppStream identifiers.
    auto app = Gtk::Application::create("de.speedprog.comfort-select-dashboard");

    app->signal_activate().connect([&app]() -> void {
        // A second launch activates the running instance: bring the existing window forward
        // instead of creating a second one.
        auto const windows = app->get_windows();
        if (!windows.empty()) {
            windows.front()->present();
            return;
        }

        try {
            std::string const data_dir = Glib::get_user_data_dir() + "/libscale";
            std::filesystem::create_directories(data_dir);

            // Data Privacy: Keep the application directory owner-only as well.
            std::filesystem::permissions(data_dir, static_cast<std::filesystem::perms>(dir_perms),
                                         std::filesystem::perm_options::replace);

            // Initialize Logging
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                data_dir + "/app.log", log_max_size, log_max_files);
            auto logger = std::make_shared<spdlog::logger>("libscale", file_sink);
            spdlog::set_default_logger(logger);
            spdlog::info("Application starting...");

            // Initialize dependencies
            static libscale::SqliteDatabaseManager db_manager;
            std::string const db_path = data_dir + "/data.db";
            if (!db_manager.initialize(db_path)) {
                throw std::runtime_error("Failed to initialize database at " + db_path);
            }

            // Data Privacy: Enforce strict permissions (chmod 600)
            if (std::filesystem::exists(db_path)) {
                std::filesystem::permissions(
                    db_path,
                    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                    std::filesystem::perm_options::replace);
            }

            static std::unique_ptr<libscale::IScaleDevice> device =
                libscale::ScaleProviderFactory::create(
                    libscale::ScaleProviderFactory::ProviderType::UsbComfortSelect);
            static libscale::HealthManager manager(db_manager, *device);

            static comfort_select::JsonSettingsManager settings;
            settings.load(data_dir + "/settings.json");

            // Create and show window
            auto* window = comfort_select::MainWindow::create(manager, settings, *device);
            app->add_window(*window);
            window->set_visible(true);

            // Connect physical device hotplug arrival events to the UI thread-safely.
            // Since the USB hotplug event thread runs in the background, we must bridge
            // the event to the main GTK thread via MainWindow's dispatcher.
            // MainWindow's destructor detaches this callback before the window disappears.
            device->set_device_arrived_callback(
                [window]() -> void { window->notify_device_arrived(); });
        } catch (const std::exception& error) {
            std::cerr << "Fatal error during startup: " << error.what() << '\n';
            std::exit(1);  // NOLINT(concurrency-mt-unsafe): Terminating the process on fatal
                           // startup error inside single-threaded UI initialization is safe.
        }
    });

    return app->run(argc, argv);
}
