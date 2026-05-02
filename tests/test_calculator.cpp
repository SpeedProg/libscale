#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

#include "libscale/calculator.hpp"
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
