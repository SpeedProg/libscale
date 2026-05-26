#include <glibmm/miscutils.h>
#include <gtkmm/application.h>  // NOLINT(misc-include-cleaner)
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "gui/main_window.hpp"
#include "gui/settings.hpp"
#include "libscale/database_impl.hpp"
#include "libscale/device.hpp"  // NOLINT(misc-include-cleaner)
#include "libscale/manager.hpp"
#include "libscale/scale_factory.hpp"

static constexpr int log_max_size = 1024 * 1024 * 5;
static constexpr int log_max_files = 3;
static constexpr int dir_perms = 0700;

auto main(int argc, char* argv[]) -> int {
    // Single-Instance enforcement via App ID
    auto app = Gtk::Application::create("com.comfortselect.dashboard");

    app->signal_activate().connect([&app]() -> void {
        try {
            std::string const data_dir = Glib::get_user_data_dir() + "/libscale";
            std::filesystem::create_directories(data_dir);

            // Initialize Logging
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                data_dir + "/app.log", log_max_size, log_max_files);
            auto logger = std::make_shared<spdlog::logger>("libscale", file_sink);
            spdlog::set_default_logger(logger);
            spdlog::info("Application starting...");

            // Initialize dependencies
            static libscale::SqliteDatabaseManager db_manager;
            std::string const db_path = data_dir + "/data.db";
            db_manager.initialize(db_path);

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

            static libscale::JsonSettingsManager settings;
            settings.load(data_dir + "/settings.json");

            // Create and show window
            auto* window = libscale::MainWindow::create(manager, settings);
            app->add_window(*window);
            window->set_visible(true);

            // Connect physical device hotplug arrival events to the UI thread-safely.
            // Since the USB hotplug event thread runs in the background, we must bridge
            // the event to the main GTK thread via MainWindow's dispatcher.
            device->signal_device_arrived().connect(
                [window]() -> void { window->notify_device_arrived(); });
        } catch (const std::exception& error) {
            std::cerr << "Fatal error during startup: " << error.what() << '\n';
            std::exit(1);  // NOLINT(concurrency-mt-unsafe): Terminating the process on fatal
                           // startup error inside single-threaded UI initialization is safe.
        }
    });

    return app->run(argc, argv);
}
