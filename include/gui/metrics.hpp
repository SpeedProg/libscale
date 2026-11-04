#pragma once

#include "libscale/types.hpp"

namespace comfort_select {

// Display policy for the V1/V2 formula toggle: the calculator leaves V2 metrics at zero when
// they cannot be computed (for example when the impedance inputs are missing), so a zero V2
// value is treated as "unavailable" and the V1 value is shown instead. Keeping the rule in one
// place guarantees the dashboard, history list and graphs agree.

/**
 * @brief Returns the body fat percentage to display for the active formula version.
 * @param measurement Measurement to read from.
 * @param use_v2 True to prefer the V2 formula result when it was computed.
 * @return Effective body fat percentage.
 */
[[nodiscard]] inline auto effective_body_fat(const libscale::Measurement& measurement, bool use_v2)
    -> libscale::units::Percent {
    if (use_v2 && measurement.body_fat_v2.value > 0.0) {
        return measurement.body_fat_v2;
    }
    return measurement.body_fat;
}

/**
 * @brief Returns the body water percentage to display for the active formula version.
 * @param measurement Measurement to read from.
 * @param use_v2 True to prefer the V2 formula result when it was computed.
 * @return Effective body water percentage.
 */
[[nodiscard]] inline auto effective_water(const libscale::Measurement& measurement, bool use_v2)
    -> libscale::units::Percent {
    if (use_v2 && measurement.water_v2.value > 0.0) {
        return measurement.water_v2;
    }
    return measurement.water;
}

/**
 * @brief Returns the muscle mass percentage to display for the active formula version.
 * @param measurement Measurement to read from.
 * @param use_v2 True to prefer the V2 formula result when it was computed.
 * @return Effective muscle mass percentage.
 */
[[nodiscard]] inline auto effective_muscle_mass(const libscale::Measurement& measurement,
                                                bool use_v2) -> libscale::units::Percent {
    if (use_v2 && measurement.muscle_mass_v2.value > 0.0) {
        return measurement.muscle_mass_v2;
    }
    return measurement.muscle_mass;
}

}  // namespace comfort_select
