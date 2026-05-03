#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>

#include "libscale/calculator.hpp"
#include "libscale/database_impl.hpp"
#include "libscale/types.hpp"

using namespace libscale;

namespace {
constexpr double expected_bmi = 26.388;
constexpr double bmi_tolerance = 0.001;
constexpr int test_age = 33;
constexpr int birth_year = 1990;
constexpr double expected_weight = 85.5;
constexpr double expected_fat = 19.4;
constexpr double fat_tolerance = 0.1;
constexpr double weight_tolerance = 0.01;
}  // namespace

TEST(CalculatorTest, BMI) {  // NOLINT
    static constexpr double test_weight = 85.5;
    static constexpr int test_height = 180;
    EXPECT_NEAR(health_utils::calculate_bmi(units::Kg{test_weight}, units::Cm{test_height}),
                expected_bmi, bmi_tolerance);
}

TEST(CalculatorTest, Age) {  // NOLINT
    using namespace std::chrono;
    const year_month_day birthday = 1990y / 1 / 1;
    const year_month_day today = 2023y / 10 / 1;
    const auto age = health_utils::calculate_age(birthday, today);
    ASSERT_TRUE(age.has_value());
    if (age) {
        EXPECT_EQ(age->value, test_age);
    }
}

TEST(CalculatorTest, BioImpedance) {  // NOLINT
    static constexpr int test_height = 180;
    static constexpr int test_activity = 3;
    static constexpr int test_year = 2023;
    static constexpr int test_month = 10;
    static constexpr int test_day = 1;
    static constexpr int test_imp50k = 450;
    static constexpr int test_imp5k = 500;
    static constexpr int test_weight_raw = 855;

    UserProfile user;
    user.height = units::Cm{test_height};
    user.gender = Gender::Male;
    user.activity_level = static_cast<ActivityLevel>(test_activity);
    user.birthday = std::chrono::year(birth_year) / 1 / 1;

    RawMeasurement raw{};
    raw.year = test_year;
    raw.month = test_month;
    raw.day = test_day;
    raw.imp50k = test_imp50k;
    raw.imp5k = test_imp5k;
    raw.weight_raw = test_weight_raw;

    const BioImpedanceCalculatorV1 calculator;
    const auto measurement = calculator.calculate(raw, user);

    ASSERT_TRUE(measurement.has_value());
    if (measurement) {
        EXPECT_NEAR(measurement->weight.value, expected_weight, weight_tolerance);
        EXPECT_NEAR(measurement->body_fat.value, expected_fat, fat_tolerance);
    }
}

TEST(DatabaseTest, RawDataPersistence) {  // NOLINT
    static constexpr double test_weight = 75.5;
    static constexpr double test_fat = 15.2;
    static constexpr double test_water = 55.4;
    static constexpr double test_muscle = 40.1;
    static constexpr double test_bmi = 22.5;
    static constexpr int test_age = 30;
    static constexpr uint16_t test_weight_raw = 755;
    static constexpr uint16_t test_imp5k = 512;
    static constexpr uint16_t test_imp50k = 480;
    static constexpr int test_year = 2023;
    static constexpr int test_month = 10;
    static constexpr int test_day = 1;

    std::filesystem::path const db_path = "test_temp.db";
    // Clean up any stale test database
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    SqliteDatabaseManager db_manager;
    ASSERT_TRUE(db_manager.initialize(db_path));

    // 1. Add a test user
    UserProfile user;
    user.name = "Test User";
    user.device_user_id = "slot_1";
    db_manager.add_user(user);

    auto users = db_manager.get_users();
    ASSERT_FALSE(users.empty());
    int const user_id = users.front().id;

    // 2. Add a measurement with raw data populated
    Measurement measurement;
    measurement.user_id = user_id;
    measurement.weight = units::Kg{test_weight};
    measurement.body_fat = units::Percent{test_fat};
    measurement.water = units::Percent{test_water};
    measurement.muscle_mass = units::Percent{test_muscle};
    measurement.bmi = test_bmi;
    measurement.age = units::Years{test_age};
    measurement.date = std::chrono::year(test_year) / test_month / test_day;

    ScaleRawData raw_data;
    raw_data.weight_raw = test_weight_raw;
    raw_data.imp5k = test_imp5k;
    raw_data.imp50k = test_imp50k;
    measurement.scale_raw = raw_data;

    db_manager.add_measurement(measurement);

    // 3. Retrieve measurements
    auto measurements = db_manager.get_measurements(user_id);
    ASSERT_EQ(measurements.size(), 1U);
    int const meas_id = measurements.front().id;

    // 4. Retrieve and verify raw telemetry
    auto retrieved_raw = db_manager.get_scale_raw_data(meas_id);
    ASSERT_TRUE(retrieved_raw.has_value());
    if (retrieved_raw) {
        EXPECT_EQ(retrieved_raw->measurement_id, meas_id);
        EXPECT_EQ(retrieved_raw->weight_raw, test_weight_raw);
        EXPECT_EQ(retrieved_raw->imp5k, test_imp5k);
        EXPECT_EQ(retrieved_raw->imp50k, test_imp50k);
    }

    // 5. Delete measurement and verify cascading deletion of telemetry
    db_manager.delete_measurement(meas_id);
    auto deleted_raw = db_manager.get_scale_raw_data(meas_id);
    EXPECT_FALSE(deleted_raw.has_value());

    // Clean up
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }
}
