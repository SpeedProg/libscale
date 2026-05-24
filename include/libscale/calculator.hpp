#pragma once
#include <optional>
#include <string_view>

#include "biometrics.hpp"
#include "types.hpp"

namespace libscale {

/**
 * @brief Utility functions for common health metrics.
 */
namespace health_utils {
/**
 * @brief Computes the Body Mass Index (BMI).
 * @param weight_kg User's weight wrapped in a strong type.
 * @param height_cm User's height wrapped in a strong type.
 * @return Computed BMI value.
 */
auto calculate_bmi(units::Kg weight_kg, units::Cm height_cm) -> double;

/**
 * @brief Calculates age in years from a birthday string (YYYY-MM-DD).
 * @param birthday User's birthday.
 * @param today Reference date (usually today).
 * @return Calculated age in years.
 */
auto calculate_age(std::chrono::year_month_day birthday, std::chrono::year_month_day today)
    -> std::optional<units::Years>;
}  // namespace health_utils

/**
 * @brief Interface for Bio-Impedance calculations.
 * Enables dependency inversion and support for multiple formula versions.
 */
class IBioImpedanceCalculator {
   public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~IBioImpedanceCalculator() = default;

    /**
     * @brief Default constructor.
     */
    IBioImpedanceCalculator() = default;

    IBioImpedanceCalculator(const IBioImpedanceCalculator&) = delete;
    auto operator=(const IBioImpedanceCalculator&) -> IBioImpedanceCalculator& = delete;
    IBioImpedanceCalculator(IBioImpedanceCalculator&&) = delete;
    auto operator=(IBioImpedanceCalculator&&) -> IBioImpedanceCalculator& = delete;

    /**
     * @brief Computes high-level health metrics from raw scale values.
     * @param raw The raw measurement containing impedance and weight.
     * @param user The user profile containing physical attributes.
     * @return Computed Measurement struct if successful, std::nullopt otherwise.
     */
    [[nodiscard]] virtual auto calculate(const RawMeasurement& raw, const UserProfile& user) const
        -> std::optional<Measurement> = 0;
};

/**
 * @brief Concrete implementation of IBioImpedanceCalculator using V1 algorithm formulas.
 */
class BioImpedanceCalculatorV1 : public IBioImpedanceCalculator {
   public:
    /// Version identifier of the calculation formula engine.
    static constexpr int version = 1;

    /**
     * @brief Computes high-level health metrics using the V1 bio-impedance algorithm.
     * @param raw The raw measurement.
     * @param user The user profile.
     * @return Computed Measurement struct if successful, std::nullopt otherwise.
     */
    [[nodiscard]] auto calculate(const RawMeasurement& raw, const UserProfile& user) const
        -> std::optional<Measurement> override;

   private:
    /**
     * @brief Internal helper context containing biometrics and pre-calculated variables
     * used during the multi-stage bio-impedance computation.
     */
    struct CalculationContext {
        Biometrics bio;
        double bmi = 0.0;
        double height_sq = 0.0;
        units::Kg weight;
        units::Ohm imp50k;
        units::Ohm imp5k;
    };

    static auto calculate_body_fat(const CalculationContext& ctx) -> units::Percent;
    static auto calculate_muscle_mass(const CalculationContext& ctx) -> units::Percent;
    static auto calculate_water_content(const CalculationContext& ctx) -> units::Percent;
    static auto calculate_energy_needs(const CalculationContext& ctx, units::Percent muscle_mass)
        -> units::Kcal;

    static auto calculate_body_fat_v2(const CalculationContext& ctx, double& rfm_out)
        -> units::Percent;
    static auto calculate_water_content_v2(const CalculationContext& ctx, units::Percent& ecw_out)
        -> units::Percent;
    static auto calculate_muscle_mass_v2(const CalculationContext& ctx) -> units::Percent;
    static auto calculate_energy_needs_v2(const CalculationContext& ctx) -> units::Kcal;

    // Architectural Refactoring: Helper methods to consolidate branching logic
    [[nodiscard]] static auto get_activity_multiplier(ActivityLevel activity_level) -> double;
    [[nodiscard]] static auto get_fat_adjustment(Gender gender, ActivityLevel activity_level)
        -> double;
    [[nodiscard]] static auto get_muscle_offset(Gender gender, ActivityLevel activity_level)
        -> double;
    [[nodiscard]] static auto get_water_offset(Gender gender, ActivityLevel activity_level)
        -> double;
};

}  // namespace libscale
