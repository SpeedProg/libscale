#include "libscale/calculator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>

#include "formula_config.hpp"
#include "libscale/types.hpp"
#include "libscale/units.hpp"

namespace libscale {

using internal::FormulaConfig;

namespace {  // Internal helper utilities

auto round_to_one_decimal(double val) -> double {
    return std::round(val * FormulaConfig::rounding_factor) / FormulaConfig::rounding_factor;
}

// Utility to round units::Percent
auto round_percent(units::Percent percent) -> units::Percent {
    return units::Percent{round_to_one_decimal(percent.value)};
}

}  // namespace

namespace health_utils {

auto calculate_bmi(units::Kg weight_kg, units::Cm height_cm) -> double {
    if (height_cm.value <= 0 || weight_kg.value <= 0) {
        return 0.0;
    }
    double const height_m = static_cast<double>(height_cm.value) / FormulaConfig::cm_per_m;
    return weight_kg.value / (height_m * height_m);
}

auto calculate_age(std::chrono::year_month_day birthday, std::chrono::year_month_day today)
    -> std::optional<units::Years> {
    if (!birthday.ok() || !today.ok()) {
        return std::nullopt;
    }

    int age = static_cast<int>(today.year()) - static_cast<int>(birthday.year());
    if (today.month() < birthday.month() ||
        (today.month() == birthday.month() && today.day() < birthday.day())) {
        age--;
    }
    return units::Years{age};
}

}  // namespace health_utils

auto BioImpedanceCalculatorV1::calculate(const RawMeasurement& raw, const UserProfile& user,
                                         ImpedancePathway pathway) const
    -> std::optional<Measurement> {
    const auto measurement_date = std::chrono::year(raw.year) / raw.month / raw.day;

    const auto age_opt = health_utils::calculate_age(user.birthday, measurement_date);
    if (!age_opt) {
        return std::nullopt;
    }

    CalculationContext ctx;
    ctx.weight = raw.weight();
    ctx.bio = Biometrics::from_user(user, *age_opt);
    ctx.pathway = pathway;

    // Defensive Programming: Validate domain invariants
    if (!ctx.bio.is_valid()) {
        return std::nullopt;
    }

    ctx.bmi = health_utils::calculate_bmi(ctx.weight, ctx.bio.height);
    const double height_m = static_cast<double>(ctx.bio.height.value) / FormulaConfig::cm_per_m;
    ctx.height_sq = height_m * height_m;

    auto imp50k_val = static_cast<double>(raw.imp50k);
    auto imp5k_val = static_cast<double>(raw.imp5k);
    if (pathway == ImpedancePathway::FootToFoot) {
        imp50k_val *= FormulaConfig::Pathway::foot_to_foot_beta;
        imp5k_val *= FormulaConfig::Pathway::foot_to_foot_beta;
    }
    ctx.imp50k = units::Ohm{imp50k_val};
    ctx.imp5k = units::Ohm{imp5k_val};

    Measurement measurement;
    measurement.user_id = user.id;
    measurement.weight = ctx.weight;
    measurement.bmi = ctx.bmi;
    measurement.age = ctx.bio.age;
    measurement.date = measurement_date;

    measurement.body_fat = round_percent(calculate_body_fat(ctx));
    measurement.muscle_mass = round_percent(calculate_muscle_mass(ctx));
    measurement.water = round_percent(calculate_water_content(ctx));
    measurement.energy = calculate_energy_needs(ctx, measurement.muscle_mass);

    measurement.waist_circumference = user.waist_circumference;
    double rfm_val = 0.0;
    measurement.body_fat_v2 = round_percent(calculate_body_fat_v2(ctx, rfm_val));
    measurement.rfm = round_to_one_decimal(rfm_val);
    units::Percent ecw_val{0.0};
    measurement.water_v2 = round_percent(calculate_water_content_v2(ctx, ecw_val));
    measurement.ecw_v2 = round_percent(ecw_val);
    measurement.muscle_mass_v2 = round_percent(calculate_muscle_mass_v2(ctx));
    measurement.energy_v2 = calculate_energy_needs_v2(ctx);

    measurement.scale_raw = ScaleRawData{.id = 0,
                                         .measurement_id = 0,
                                         .weight_raw = raw.weight_raw,
                                         .imp5k = raw.imp5k,
                                         .imp50k = raw.imp50k};
    measurement.formula_version = version;

    return measurement;
}

auto BioImpedanceCalculatorV1::calculate_body_fat(const CalculationContext& ctx) -> units::Percent {
    using Config = FormulaConfig::Fat;

    // Biological Principle: Body fat estimation is structured around the Deurenberg equations,
    // which adapt calculations based on age limits to reflect pediatric vs. adult physiological
    // profiles.
    // - For children (< 17 years), the relationship between BMI, age, and 50 kHz bio-impedance is
    // highly linear.
    // - For adults, distinct coefficient slopes are applied based on biological gender, activity
    // levels, and
    //   absolute limits (5% male, 10% female) to avoid underestimating essential structural body
    //   fat.
    if (ctx.bio.age.value < FormulaConfig::child_age_limit) {
        const double fat = (Config::Child::bmi_coeff * ctx.bmi) +
                           (Config::Child::age_coeff * ctx.bio.age.value) +
                           (Config::Child::imp_coeff * ctx.imp50k.value) - Config::Child::offset;
        return units::Percent{fat};
    }

    const double age_coeff = (ctx.bio.gender == Gender::Male) ? Config::Adult::male_age_coeff
                                                              : Config::Adult::female_age_coeff;
    const double offset = (ctx.bio.gender == Gender::Male) ? Config::Adult::male_offset
                                                           : Config::Adult::female_offset;
    const double limit =
        (ctx.bio.gender == Gender::Male) ? Config::Adult::male_limit : Config::Adult::female_limit;

    const double base_fat = (Config::Adult::bmi_coeff * ctx.bmi) + (age_coeff * ctx.bio.age.value) +
                            (Config::Adult::imp_coeff * ctx.imp50k.value) - offset;
    const double adjusted_fat =
        base_fat - get_fat_adjustment(ctx.bio.gender, ctx.bio.activity_level);

    return units::Percent{std::max(adjusted_fat, limit)};
}

auto BioImpedanceCalculatorV1::calculate_muscle_mass(const CalculationContext& ctx)
    -> units::Percent {
    using Config = FormulaConfig::Muscle;
    if (ctx.imp50k.value <= 0 || ctx.imp5k.value <= 0 || ctx.weight.value <= 0) {
        return units::Percent{0.0};
    }

    // Physical Principle: Multi-Frequency Bioelectrical Impedance Analysis (MF-BIA).
    // Low-frequency AC currents (5 kHz) cannot penetrate the capacitive barrier of cell membranes,
    // flowing exclusively through extracellular water (ECW). In contrast, high-frequency currents
    // (50 kHz) freely cross cell membranes, flowing through both intracellular water (ICW) and ECW.
    // By taking the differential ratio between 50 kHz and 5 kHz impedance relative to the physical
    // height-squared ($H^2$) geometry, the model accurately isolates skeletal muscle mass from
    // fluid shifts.
    const double offset = get_muscle_offset(ctx.bio.gender, ctx.bio.activity_level);
    const double height_cm_sq = ctx.height_sq * FormulaConfig::cm_per_m * FormulaConfig::cm_per_m;
    const double term1 =
        (Config::imp50k_coeff / ctx.imp50k.value - Config::imp5k_coeff / ctx.imp5k.value) *
        height_cm_sq;

    const double muscle =
        (term1 + Config::weight_coeff * ctx.weight.value -
         Config::age_coeff * static_cast<double>(ctx.bio.age.value) + Config::base_const + offset) /
        ctx.weight.value * FormulaConfig::percentage_factor;

    return units::Percent{muscle};
}

auto BioImpedanceCalculatorV1::calculate_water_content(const CalculationContext& ctx)
    -> units::Percent {
    using Config = FormulaConfig::Water;
    if (ctx.imp50k.value <= 0 || ctx.weight.value <= 0) {
        return units::Percent{0.0};
    }

    // Mathematical Principle: The Impedance Index ($H^2/Z_{50}$).
    // Volume ($V$) of a conductor is mathematically proportional to $L^2/R$ (where $L$ is
    // length/height and $R$ is resistance/impedance). Total Body Water (TBW) is modeled by scaling
    // the height-squared over the 50 kHz impedance ($H^2/Z_{50}$) index, adjusted by total body
    // weight, age, biological gender, and metabolic offsets from exercise-induced muscle cell
    // hydration levels.
    const double water_offset = get_water_offset(ctx.bio.gender, ctx.bio.activity_level);
    const double height_cm_sq = ctx.height_sq * FormulaConfig::cm_per_m * FormulaConfig::cm_per_m;
    const double water = (Config::imp50k_coeff * height_cm_sq / ctx.imp50k.value +
                          Config::weight_coeff * ctx.weight.value -
                          Config::age_coeff * static_cast<double>(ctx.bio.age.value) +
                          Config::base_const + water_offset) /
                         ctx.weight.value * FormulaConfig::percentage_factor;

    return units::Percent{water};
}

auto BioImpedanceCalculatorV1::calculate_energy_needs(const CalculationContext& ctx,
                                                      units::Percent muscle_mass) -> units::Kcal {
    using Config = FormulaConfig::Energy;

    // Metabolic Model: The Katch-McArdle Basal Metabolic Rate (BMR) Formulation.
    // Unlike standard formulas that rely solely on overall weight (which miscalculates for athletic
    // or obese individuals), Katch-McArdle computes BMR from Lean Body Mass (LBM).
    // Here, LBM is isolated using the calculated muscle mass percentage multiplied by total weight.
    // The resulting BMR is scaled by the physical activity multiplier (representing Total Daily
    // Energy Expenditure - TDEE) to determine daily caloric needs.
    const double activity_mult = get_activity_multiplier(ctx.bio.activity_level);
    const double bmr =
        (Config::muscle_weight_coeff * muscle_mass.value * ctx.weight.value + Config::base_const) *
        activity_mult;

    return units::Kcal{static_cast<int>(std::round(bmr))};
}

auto BioImpedanceCalculatorV1::calculate_body_fat_v2(const CalculationContext& ctx, double& rfm_out)
    -> units::Percent {
    using Config = FormulaConfig::V2::Fat;

    if (ctx.bio.waist_circumference && ctx.bio.waist_circumference->value > 0) {
        const auto waist_cm = static_cast<double>(ctx.bio.waist_circumference->value);
        const auto height_cm = static_cast<double>(ctx.bio.height.value);
        const double base =
            (ctx.bio.gender == Gender::Male) ? Config::male_rfm_base : Config::female_rfm_base;
        rfm_out = base - Config::rfm_multiplier * (height_cm / waist_cm);
    } else {
        rfm_out = 0.0;
    }

    if (ctx.imp50k.value <= 0 || ctx.weight.value <= 0) {
        return units::Percent{0.0};
    }
    const double height_cm_sq = ctx.height_sq * FormulaConfig::cm_per_m * FormulaConfig::cm_per_m;
    const double gender_val = (ctx.bio.gender == Gender::Male) ? 1.0 : 0.0;
    double fat_free_mass = Config::kyle_base +
                           (Config::kyle_h2z_coeff * height_cm_sq / ctx.imp50k.value) +
                           (Config::kyle_weight_coeff * ctx.weight.value) +
                           (Config::kyle_age_coeff * static_cast<double>(ctx.bio.age.value)) +
                           (Config::kyle_gender_coeff * gender_val);

    fat_free_mass *= calculate_whtr_factor(ctx);

    const double fat_mass = ctx.weight.value - fat_free_mass;
    const double fat_pct = (fat_mass / ctx.weight.value) * FormulaConfig::percentage_factor;
    const double limit =
        (ctx.bio.gender == Gender::Male) ? Config::male_limit : Config::female_limit;
    return units::Percent{std::max(fat_pct, limit)};
}

auto BioImpedanceCalculatorV1::calculate_water_content_v2(const CalculationContext& ctx,
                                                          units::Percent& ecw_out)
    -> units::Percent {
    using Config = FormulaConfig::V2::Water;
    if (ctx.imp50k.value <= 0 || ctx.imp5k.value <= 0 || ctx.weight.value <= 0) {
        ecw_out = units::Percent{0.0};
        return units::Percent{0.0};
    }
    const double height_cm_sq = ctx.height_sq * FormulaConfig::cm_per_m * FormulaConfig::cm_per_m;

    const double extra_cellular_water = (Config::ecw_h2z_coeff * (height_cm_sq / ctx.imp5k.value)) +
                                        (Config::ecw_weight_coeff * ctx.weight.value) +
                                        Config::ecw_offset;

    const double total_body_water = (Config::tbw_h2z_coeff * (height_cm_sq / ctx.imp50k.value)) +
                                    (Config::tbw_weight_coeff * ctx.weight.value) +
                                    Config::tbw_offset;

    ecw_out = units::Percent{(extra_cellular_water / ctx.weight.value) *
                             FormulaConfig::percentage_factor};
    return units::Percent{(total_body_water / ctx.weight.value) * FormulaConfig::percentage_factor};
}

auto BioImpedanceCalculatorV1::calculate_muscle_mass_v2(const CalculationContext& ctx)
    -> units::Percent {
    using Config = FormulaConfig::V2::Muscle;
    if (ctx.imp50k.value <= 0 || ctx.weight.value <= 0) {
        return units::Percent{0.0};
    }
    const double height_cm_sq = ctx.height_sq * FormulaConfig::cm_per_m * FormulaConfig::cm_per_m;
    const double gender_val = (ctx.bio.gender == Gender::Male) ? 1.0 : 0.0;

    double skeletal_muscle_mass =
        (Config::kim_h2z_coeff * (height_cm_sq / ctx.imp50k.value)) +
        (Config::kim_weight_coeff * ctx.weight.value) + (Config::kim_gender_coeff * gender_val) +
        (Config::kim_age_coeff * static_cast<double>(ctx.bio.age.value)) + Config::kim_offset;

    skeletal_muscle_mass *= calculate_whtr_factor(ctx);

    return units::Percent{(skeletal_muscle_mass / ctx.weight.value) *
                          FormulaConfig::percentage_factor};
}

auto BioImpedanceCalculatorV1::calculate_energy_needs_v2(const CalculationContext& ctx)
    -> units::Kcal {
    using Config = FormulaConfig::V2::Energy;
    const double gender_offset =
        (ctx.bio.gender == Gender::Male) ? Config::male_offset : Config::female_offset;
    const double activity_mult = get_activity_multiplier(ctx.bio.activity_level);

    const double basal_metabolic_rate =
        (Config::weight_coeff * ctx.weight.value) +
        (Config::height_coeff * static_cast<double>(ctx.bio.height.value)) -
        (Config::age_coeff * static_cast<double>(ctx.bio.age.value)) + gender_offset;

    return units::Kcal{static_cast<int>(std::round(basal_metabolic_rate * activity_mult))};
}

auto BioImpedanceCalculatorV1::get_activity_multiplier(ActivityLevel activity_level) -> double {
    using Config = FormulaConfig::Energy;
    switch (activity_level) {
        case ActivityLevel::Scarcely:
            return Config::mult_scarcely;
        case ActivityLevel::Light:
            return Config::mult_light;
        case ActivityLevel::HomeGarden:
            return Config::mult_home;
        case ActivityLevel::Sports:
            return Config::mult_sports;
        case ActivityLevel::HighLevel:
            return Config::mult_high;
        default:
            return Config::mult_default;
    }
}

auto BioImpedanceCalculatorV1::get_fat_adjustment(Gender gender, ActivityLevel activity_level)
    -> double {
    using Config = FormulaConfig::Fat::ActivityAdj;
    if (activity_level == ActivityLevel::Sports) {
        return (gender == Gender::Male) ? Config::male_sports : Config::female_sports;
    }
    if (activity_level == ActivityLevel::HighLevel) {
        return (gender == Gender::Male) ? Config::male_high : Config::female_high;
    }
    return 0.0;
}

auto BioImpedanceCalculatorV1::get_muscle_offset(Gender gender, ActivityLevel activity_level)
    -> double {
    using Config = FormulaConfig::Muscle;
    if (gender == Gender::Female) {
        if (activity_level == ActivityLevel::HighLevel) {
            return Config::female_high_offset;
        }
        return 0.0;
    }
    if (activity_level == ActivityLevel::Sports) {
        return Config::male_sports_offset;
    }
    if (activity_level == ActivityLevel::HighLevel) {
        return Config::male_high_offset;
    }
    return Config::male_base_offset;
}

auto BioImpedanceCalculatorV1::get_water_offset(Gender gender, ActivityLevel activity_level)
    -> double {
    using Config = FormulaConfig::Water;
    if (gender == Gender::Female) {
        if (activity_level == ActivityLevel::Sports) {
            return Config::female_sports_offset;
        }
        if (activity_level == ActivityLevel::HighLevel) {
            return Config::female_high_offset;
        }
        return 0.0;
    }
    if (activity_level == ActivityLevel::Sports) {
        return Config::male_sports_offset;
    }
    if (activity_level == ActivityLevel::HighLevel) {
        return Config::male_high_offset;
    }
    return Config::male_base_offset;
}

auto BioImpedanceCalculatorV1::calculate_whtr_factor(const CalculationContext& ctx) -> double {
    if (ctx.pathway == ImpedancePathway::FootToFoot && ctx.bio.waist_circumference &&
        ctx.bio.waist_circumference->value > 0 && ctx.bio.height.value > 0) {
        const double whtr = static_cast<double>(ctx.bio.waist_circumference->value) /
                            static_cast<double>(ctx.bio.height.value);
        if (whtr > FormulaConfig::Pathway::whtr_threshold) {
            return 1.0 - (FormulaConfig::Pathway::whtr_alpha *
                          (whtr - FormulaConfig::Pathway::whtr_threshold));
        }
    }
    return 1.0;
}

}  // namespace libscale
