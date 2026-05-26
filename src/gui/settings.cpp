#include "gui/settings.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace libscale {
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): Subscripting is standard for accessing/inserting keys in nlohmann::json.

static constexpr int json_indent_size = 4;

void JsonSettingsManager::load(const std::filesystem::path& config_path) {
    m_path = config_path;
    if (!std::filesystem::exists(m_path)) {
        return;
    }

    try {
        std::ifstream input_file(m_path);
        nlohmann::json settings_json;
        input_file >> settings_json;

        if (settings_json.contains("units")) {
            std::string const unit_str = settings_json["units"];
            m_units = (unit_str == "imperial") ? UnitSystem::Imperial : UnitSystem::Metric;
        }
        if (settings_json.contains("last_user_id")) {
            m_last_user_id = settings_json["last_user_id"];
        }
        if (settings_json.contains("win_width")) {
            m_window_width = settings_json["win_width"];
        }
        if (settings_json.contains("win_height")) {
            m_window_height = settings_json["win_height"];
        }
        if (settings_json.contains("win_max")) {
            m_is_maximized = settings_json["win_max"];
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to load settings from {}: {}", m_path.string(), e.what());
    }
}

void JsonSettingsManager::save() {
    try {
        nlohmann::json settings_json;

        settings_json["units"] = (m_units == UnitSystem::Imperial) ? "imperial" : "metric";
        settings_json["last_user_id"] = m_last_user_id;
        settings_json["win_width"] = m_window_width;
        settings_json["win_height"] = m_window_height;
        settings_json["win_max"] = m_is_maximized;

        std::ofstream output_file(m_path);
        output_file << settings_json.dump(json_indent_size);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save settings to {}: {}", m_path.string(), e.what());
    }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
}  // namespace libscale
