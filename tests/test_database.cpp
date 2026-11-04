#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>

#include "libscale/database_impl.hpp"
#include "libscale/types.hpp"
#include "libscale/units.hpp"

using namespace libscale;

namespace {

auto make_temp_db_path(const std::string& name) -> std::filesystem::path {
    return std::filesystem::temp_directory_path() / name;
}

auto make_timestamp(int year, unsigned int month, unsigned int day, int hour = 0)
    -> std::chrono::sys_seconds {
    auto const date = std::chrono::sys_days{std::chrono::year(year) / month / day};
    return std::chrono::sys_seconds{date + std::chrono::hours{hour}};
}

}  // namespace

class DatabaseMeasurementTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::filesystem::remove(m_db_path);
        ASSERT_TRUE(m_database.initialize(m_db_path));

        UserProfile user;
        user.name = "Alice";
        user.height = units::Cm{170};
        user.gender = Gender::Female;
        user.activity_level = ActivityLevel::HomeGarden;
        user.birthday = std::chrono::year(1990) / 1 / 1;
        m_user_id = m_database.add_user(user);
    }

    void TearDown() override {
        std::filesystem::remove(m_db_path);
        std::filesystem::remove(make_backup_path());
    }

    [[nodiscard]] auto make_backup_path() const -> std::filesystem::path {
        return std::filesystem::path{m_db_path}.concat(".tmp_backup");
    }

    [[nodiscard]] auto make_measurement(std::chrono::sys_seconds date, double weight_kg) const
        -> Measurement {
        Measurement measurement;
        measurement.user_id = m_user_id;
        measurement.date = date;
        measurement.weight = units::Kg{weight_kg};
        return measurement;
    }

    std::filesystem::path const m_db_path = make_temp_db_path("libscale_test_database.db");
    SqliteDatabaseManager m_database;
    int m_user_id = 0;
};

TEST_F(DatabaseMeasurementTest /*unused*/, RangeQueryIncludesBothBounds) {
    m_database.add_measurement(make_measurement(make_timestamp(2026, 5, 20, 8), 70.0));
    m_database.add_measurement(make_measurement(make_timestamp(2026, 5, 22, 12), 71.0));
    m_database.add_measurement(make_measurement(make_timestamp(2026, 5, 23), 72.0));

    auto const start = std::chrono::year(2026) / 5 / 20;
    auto const end = std::chrono::year(2026) / 5 / 22;
    auto const range = m_database.get_measurements_range(m_user_id, start, end);

    ASSERT_EQ(range.size(), 2U);
    EXPECT_EQ(range.front().date, make_timestamp(2026, 5, 20, 8));
    EXPECT_EQ(range.back().date, make_timestamp(2026, 5, 22, 12));

    auto const single_day = m_database.get_measurements_range(m_user_id, end, end);
    ASSERT_EQ(single_day.size(), 1U);
    EXPECT_EQ(single_day.front().date, make_timestamp(2026, 5, 22, 12));

    auto const empty_day = m_database.get_measurements_range(
        m_user_id, std::chrono::year(2026) / 5 / 21, std::chrono::year(2026) / 5 / 21);
    EXPECT_TRUE(empty_day.empty());
}

TEST_F(DatabaseMeasurementTest /*unused*/, IdenticalMeasurementIsNotStoredTwice) {
    auto measurement = make_measurement(make_timestamp(2026, 5, 22, 12), 70.0);
    measurement.scale_raw = ScaleRawData{.weight_raw = 700, .imp5k = 500, .imp50k = 450};

    auto const first_stored = m_database.add_measurement(measurement);
    auto const second_stored = m_database.add_measurement(measurement);

    EXPECT_EQ(first_stored, measurement.date);
    EXPECT_EQ(second_stored, measurement.date);
    EXPECT_EQ(m_database.get_measurements(m_user_id).size(), 1U);
}

TEST_F(DatabaseMeasurementTest /*unused*/, DifferentMeasurementAtSameSecondIsShifted) {
    auto const date = make_timestamp(2026, 5, 22, 12);
    m_database.add_measurement(make_measurement(date, 70.0));

    auto const shifted = m_database.add_measurement(make_measurement(date, 71.0));

    EXPECT_EQ(shifted, date + std::chrono::seconds{1});
    auto const measurements = m_database.get_measurements(m_user_id);
    ASSERT_EQ(measurements.size(), 2U);
    EXPECT_EQ(measurements[0].date, date);
    EXPECT_EQ(measurements[1].date, date + std::chrono::seconds{1});
}

TEST_F(DatabaseMeasurementTest /*unused*/, ReplacePathUpdatesMetricsAndRawTelemetry) {
    auto measurement = make_measurement(make_timestamp(2026, 5, 22, 12), 70.0);
    measurement.scale_raw = ScaleRawData{.weight_raw = 700, .imp5k = 500, .imp50k = 450};
    m_database.add_measurement(measurement);

    auto stored = m_database.get_measurements(m_user_id);
    ASSERT_EQ(stored.size(), 1U);
    int const measurement_id = stored.front().id;

    Measurement recalculated = stored.front();
    recalculated.body_fat = units::Percent{25.0};
    recalculated.scale_raw = ScaleRawData{.weight_raw = 700, .imp5k = 510, .imp50k = 460};
    m_database.add_measurement(recalculated);

    stored = m_database.get_measurements(m_user_id);
    ASSERT_EQ(stored.size(), 1U);
    EXPECT_NEAR(stored.front().body_fat.value, 25.0, 0.001);

    auto const raw = m_database.get_scale_raw_data(measurement_id);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(raw->imp5k, 510);
    EXPECT_EQ(raw->imp50k, 460);
}
