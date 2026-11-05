#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <vector>

#include "gui/dashboard_view_model.hpp"
#include "libscale/types.hpp"
#include "libscale/units.hpp"

using comfort_select::DashboardViewModel;
using libscale::Measurement;

namespace {

constexpr int default_measurement_count = 10;

auto make_measurements(int count) -> std::vector<Measurement> {
    std::vector<Measurement> data;
    for (int i = 0; i < count; ++i) {
        Measurement measurement;
        measurement.date =
            std::chrono::sys_days{std::chrono::year(2026) / 1 / 1} + std::chrono::days{i};
        measurement.weight = libscale::units::Kg{70.0 + static_cast<double>(i)};
        data.push_back(measurement);
    }
    return data;
}

}  // namespace

TEST(DashboardViewModelTest /*unused*/, SetDataResetsRangeAndShowsEverything) {
    DashboardViewModel view_model;
    view_model.set_data(make_measurements(default_measurement_count));

    EXPECT_EQ(view_model.get_filtered_data().size(), default_measurement_count);
    EXPECT_DOUBLE_EQ(view_model.get_start_percentage(), 0.0);
    EXPECT_DOUBLE_EQ(view_model.get_end_percentage(), 1.0);
}

TEST(DashboardViewModelTest /*unused*/, PercentagesFilterTheDataset) {
    DashboardViewModel view_model;
    view_model.set_data(make_measurements(default_measurement_count));

    view_model.set_range_percentages(0.0, 0.5);
    EXPECT_EQ(view_model.get_filtered_data().size(), 5U);
    EXPECT_NEAR(view_model.get_filtered_data().back().weight.value, 74.0, 0.001);

    view_model.set_range_percentages(0.5, 0.5);
    ASSERT_EQ(view_model.get_filtered_data().size(), 1U);
    EXPECT_NEAR(view_model.get_filtered_data().front().weight.value, 74.0, 0.001);
}

TEST(DashboardViewModelTest /*unused*/, SwappedPercentagesAreNormalized) {
    DashboardViewModel view_model;
    view_model.set_data(make_measurements(default_measurement_count));

    view_model.set_range_percentages(0.9, 0.1);

    EXPECT_LT(view_model.get_start_percentage(), view_model.get_end_percentage());
    EXPECT_FALSE(view_model.get_filtered_data().empty());
}

TEST(DashboardViewModelTest /*unused*/, PresetKeepsTheRequestedWindow) {
    DashboardViewModel view_model;
    view_model.set_data(make_measurements(40));

    view_model.set_range_preset(30);

    // Cutoff is 2026-01-10 (last measurement minus 30 days), so 31 of 40 measurements remain.
    EXPECT_EQ(view_model.get_filtered_data().size(), 31U);
    EXPECT_EQ(view_model.get_filtered_data().front().date,
              std::chrono::sys_days{std::chrono::year(2026) / 1 / 10});
    EXPECT_EQ(view_model.get_filtered_data().back().date,
              std::chrono::sys_days{std::chrono::year(2026) / 2 / 9});
}

TEST(DashboardViewModelTest /*unused*/, PresetZeroKeepsEverything) {
    DashboardViewModel view_model;
    view_model.set_data(make_measurements(40));

    view_model.set_range_preset(30);
    view_model.set_range_preset(0);

    EXPECT_EQ(view_model.get_filtered_data().size(), 40U);
}

TEST(DashboardViewModelTest /*unused*/, EmptyDatasetStaysEmpty) {
    DashboardViewModel view_model;

    view_model.set_range_preset(30);
    view_model.set_range_percentages(0.2, 0.8);

    EXPECT_TRUE(view_model.get_filtered_data().empty());
    EXPECT_TRUE(view_model.get_all_data().empty());
}

TEST(DashboardViewModelTest /*unused*/, DataChangedSignalIsEmitted) {
    DashboardViewModel view_model;
    int emissions = 0;
    view_model.signal_data_changed().connect([&emissions]() -> void { ++emissions; });

    view_model.set_data(make_measurements(default_measurement_count));
    view_model.set_range_percentages(0.0, 0.5);
    view_model.set_range_preset(0);

    EXPECT_EQ(emissions, 3);
}
