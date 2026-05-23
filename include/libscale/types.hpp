#pragma once

// NOLINTBEGIN(misc-non-private-member-variables-in-classes): Plain Data Transport structures utilize direct public fields to allow native structural bindings and avoid redundant accessors.

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "units.hpp"

namespace libscale {

/**
 * @brief Represents the gender of the user.
 */
enum class Gender : uint8_t { 
    /// Male
    Male = 0, 
    /// Female
    Female = 1 
};

/**
 * @brief Represents the physical activity level of a user.
 * Used as an input for energy needs and bio-impedance calculations.
 */
enum class ActivityLevel : uint8_t {
    /// Scarcely active (sedentary lifestyle, little to no exercise).
    Scarcely = 1,
    /// Lightly active (light exercise or sports 1-3 days/week).
    Light = 2,
    /// Moderately active (moderate exercise, home/garden work).
    HomeGarden = 3,
    /// Very active (hard exercise or sports 6-7 days/week).
    Sports = 4,
    /// Extremely active (heavy physical job or athletic training).
    HighLevel = 5
};

/**
 * @brief A user profile containing biometric data necessary for scale synchronization
 * and health metric calculations.
 */
struct UserProfile {
    /// Unique identifier of the user in the database.
    int id = 0;
    /// The display name of the user.
    std::string name;
    /// User's height in centimeters.
    units::Cm height{0};
    /// User's gender.
    Gender gender = Gender::Female;
    /// User's daily physical activity level.
    ActivityLevel activity_level = ActivityLevel::HomeGarden;
    /// User's birthday.
    std::chrono::year_month_day birthday{};
    /// User's waist circumference in centimeters (optional).
    std::optional<units::Cm> waist_circumference;
    /// The hardware profile slot ID on the scale (1-8).
    int slot_id = 0;
    /// The abstract scale-specific identifier (e.g. "slot_1").
    std::string device_user_id;
};

/**
 * @brief Represents a raw measurement payload read directly from the scale hardware.
 */
struct RawMeasurement {
    /// Year of the measurement (4-digit format).
    uint16_t year = 0;
    /// Month of the measurement (1-12).
    uint8_t month = 0;
    /// Day of the measurement (1-31).
    uint8_t day = 0;
    /// Bio-impedance value at 5 kHz (in ohms).
    uint16_t imp5k = 0;
    /// Bio-impedance value at 50 kHz (in ohms).
    uint16_t imp50k = 0;
    /// Raw weight value as transmitted by the hardware.
    uint16_t weight_raw = 0;

    /**
     * @brief Calculates weight from raw sensor data.
     * @return Weight in kilograms.
     */
    [[nodiscard]] auto weight() const -> units::Kg {
        static constexpr double weight_divisor = 10.0;
        return units::Kg{static_cast<double>(weight_raw) / weight_divisor};
    }
};

/**
 * @brief Represents low-level raw physical telemetry from a Comfort Select USB scale.
 */
struct ScaleRawData {
    /// Unique identifier of the raw data record.
    int id = 0;
    /// Foreign key link to the corresponding Measurement record.
    int measurement_id = 0;
    /// Raw weight value as transmitted by the hardware.
    uint16_t weight_raw = 0;
    /// Raw bio-impedance value at 5 kHz (in ohms).
    uint16_t imp5k = 0;
    /// Raw bio-impedance value at 50 kHz (in ohms).
    uint16_t imp50k = 0;
};

/**
 * @brief Contains processed biometric measurements and computed health metrics.
 */
struct Measurement {
    /// Unique identifier of the measurement in the database.
    int id = 0;
    /// Reference identifier of the user who owns this measurement.
    int user_id = 0;
    /// Transient device user ID mapping helper.
    std::string device_user_id;
    /// Timestamp of the measurement.
    std::chrono::year_month_day date{};
    /// Processed weight in kilograms.
    units::Kg weight{0.0};
    /// Calculated body fat percentage.
    units::Percent body_fat{0.0};
    /// Calculated body water percentage.
    units::Percent water{0.0};
    /// Calculated muscle mass percentage.
    units::Percent muscle_mass{0.0};
    /// Computed Body Mass Index (BMI).
    double bmi = 0.0;
    /// The user's age in years at the time of this measurement.
    units::Years age{0};
    /// Daily energy requirements in kilocalories.
    units::Kcal energy{0};
    /// User's waist circumference at the time of measurement.
    std::optional<units::Cm> waist_circumference;
    /// Calculated body fat percentage (V2).
    units::Percent body_fat_v2{0.0};
    /// Calculated body water percentage (V2).
    units::Percent water_v2{0.0};
    /// Calculated muscle mass percentage (V2).
    units::Percent muscle_mass_v2{0.0};
    /// Daily energy requirements in kilocalories (V2).
    units::Kcal energy_v2{0};
    /// Calculated extra-cellular water percentage (V2).
    units::Percent ecw_v2{0.0};
    /// Relative Fat Mass (RFM) metric.
    double rfm = 0.0;
    /// Non-serialized raw physical telemetry (Pattern 3 transient field).
    std::optional<ScaleRawData> scale_raw;
    /// Tracks which formula version was used to calculate these metrics.
    int formula_version = 0;
};

}  // namespace libscale

// NOLINTEND(misc-non-private-member-variables-in-classes)
