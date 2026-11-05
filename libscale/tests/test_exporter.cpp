#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "libscale/exporter.hpp"
#include "libscale/types.hpp"
#include "libscale/units.hpp"

using libscale::DataExporter;
using libscale::Measurement;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
namespace {

auto make_measurement() -> Measurement {
    Measurement measurement;
    measurement.date = std::chrono::sys_days{std::chrono::year(2026) / 5 / 22};
    measurement.weight = libscale::units::Kg{70.5};
    measurement.body_fat = libscale::units::Percent{25.0};
    measurement.water = libscale::units::Percent{50.0};
    measurement.muscle_mass = libscale::units::Percent{45.0};
    measurement.energy = libscale::units::Kcal{2200};
    measurement.bmi = 24.4;
    measurement.rfm = 28.1;
    measurement.body_fat_v2 = libscale::units::Percent{26.0};
    measurement.water_v2 = libscale::units::Percent{51.0};
    measurement.muscle_mass_v2 = libscale::units::Percent{44.0};
    measurement.energy_v2 = libscale::units::Kcal{2250};
    measurement.ecw_v2 = libscale::units::Percent{20.0};
    measurement.formula_version = 1;
    return measurement;
}

}  // namespace

TEST(ExporterTest /*unused*/, CsvContainsBothFormulaVersions) {
    auto const csv = DataExporter::to_csv({make_measurement()});

    EXPECT_TRUE(csv.starts_with("Date,Weight(kg)"));
    EXPECT_NE(csv.find("BodyFatV2(%)"), std::string::npos);
    EXPECT_NE(csv.find("2026-05-22,70.5,25.0,50.0,45.0,2200,24.4,28.1,26.0,51.0,44.0,2250,20.0,1"),
              std::string::npos);
    EXPECT_EQ(csv.back(), '\n');
}

TEST(ExporterTest /*unused*/, CsvWithNoDataIsHeaderOnly) {
    auto const csv = DataExporter::to_csv({});

    EXPECT_EQ(std::count(csv.begin(), csv.end(), '\n'), 1);
}

TEST(ExporterTest /*unused*/, JsonContainsBothFormulaVersions) {
    auto const json_text = DataExporter::to_json({make_measurement()});
    auto const parsed = nlohmann::json::parse(json_text);

    ASSERT_TRUE(parsed.is_array());
    ASSERT_EQ(parsed.size(), 1U);
    EXPECT_EQ(parsed[0]["date"], "2026-05-22");
    EXPECT_NEAR(parsed[0]["weight_kg"].get<double>(), 70.5, 0.001);
    EXPECT_NEAR(parsed[0]["body_fat_pct"].get<double>(), 25.0, 0.001);
    EXPECT_NEAR(parsed[0]["body_fat_v2_pct"].get<double>(), 26.0, 0.001);
    EXPECT_EQ(parsed[0]["energy_v2_kcal"].get<int>(), 2250);
    EXPECT_EQ(parsed[0]["formula_version"].get<int>(), 1);
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
