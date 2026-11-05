#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "gui/settings.hpp"

using comfort_select::JsonSettingsManager;
using comfort_select::UnitSystem;

namespace {

auto make_temp_settings_path(const std::string& name) -> std::filesystem::path {
    return std::filesystem::temp_directory_path() / name;
}

}  // namespace

TEST(SettingsTest /*unused*/, LoadOfMissingFileKeepsDefaults) {
    auto const path = make_temp_settings_path("libscale_test_missing_settings.json");
    std::filesystem::remove(path);

    JsonSettingsManager settings;
    settings.load(path);

    EXPECT_EQ(settings.get_unit_system(), UnitSystem::Metric);
    EXPECT_EQ(settings.get_last_user_id(), 0);
    EXPECT_EQ(settings.get_window_width(), comfort_select::default_window_width);
    EXPECT_EQ(settings.get_window_height(), comfort_select::default_window_height);
    EXPECT_FALSE(settings.get_is_maximized());
}

TEST(SettingsTest /*unused*/, ValuesRoundTripThroughTheFile) {
    auto const path = make_temp_settings_path("libscale_test_settings.json");
    std::filesystem::remove(path);

    JsonSettingsManager settings;
    settings.load(path);
    settings.set_unit_system(UnitSystem::Imperial);
    settings.set_last_user_id(42);
    settings.set_window_state({.width = 1234, .height = 567, .maximized = true});
    settings.save();

    JsonSettingsManager reloaded;
    reloaded.load(path);

    EXPECT_EQ(reloaded.get_unit_system(), UnitSystem::Imperial);
    EXPECT_EQ(reloaded.get_last_user_id(), 42);
    EXPECT_EQ(reloaded.get_window_width(), 1234);
    EXPECT_EQ(reloaded.get_window_height(), 567);
    EXPECT_TRUE(reloaded.get_is_maximized());

    std::filesystem::remove(path);
}
