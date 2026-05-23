#pragma once

// NOLINTBEGIN(misc-non-private-member-variables-in-classes): Structs with helper methods are treated as classes by Clang-Tidy; public variables are preferred here to prevent getter/setter boilerplate for domain models.

#include "units.hpp"
#include "types.hpp"

namespace libscale {

/**
 * @brief Encapsulates a user's biometric data for health calculations.
 * Adheres to SRP by grouping related physical attributes into a single structure.
 */
struct Biometrics {
    /// Age in years.
    units::Years age;
    /// Height in centimeters.
    units::Cm height;
    /// Gender.
    Gender gender = Gender::Female;
    /// Daily physical activity level.
    ActivityLevel activity_level = ActivityLevel::HomeGarden;
    /// User's waist circumference in centimeters (optional).
    std::optional<units::Cm> waist_circumference;

    /**
     * @brief Factory method creating a Biometrics context from a UserProfile and calculated age.
     * @param user User profile containing physical stats.
     * @param age_at_meas The user's age at the time of the measurement.
     * @return Generated Biometrics instance.
     */
    [[nodiscard]] static auto from_user(const UserProfile& user, units::Years age_at_meas) -> Biometrics {
        return {.age=age_at_meas, .height=user.height, .gender=user.gender, .activity_level=user.activity_level, .waist_circumference=user.waist_circumference};
    }

    /**
     * @brief Validates physical properties inside the context (non-negative age, valid height and age ranges).
     * @return True if valid, false otherwise.
     */
    [[nodiscard]] auto is_valid() const -> bool {
        static constexpr int max_age = 150;
        return height.value > 0 && age.value >= 0 && age.value < max_age;
    }
};

} // namespace libscale

// NOLINTEND(misc-non-private-member-variables-in-classes)
