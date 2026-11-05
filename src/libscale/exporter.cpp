
#include "libscale/exporter.hpp"

#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "libscale/types.hpp"

namespace libscale {

auto DataExporter::to_csv(const std::vector<Measurement>& data) -> std::string {
    std::string csv =
        "Date,Weight(kg),BodyFat(%),Water(%),Muscle(%),Energy(kcal),BMI,RFM,"
        "BodyFatV2(%),WaterV2(%),MuscleV2(%),EnergyV2(kcal),ECW_V2(%),Version\n";
    for (const auto& measurement : data) {
        csv += std::format(
            "{:%F},{:.1f},{:.1f},{:.1f},{:.1f},{},{:.1f},{:.1f},{:.1f},{:.1f},"
            "{:.1f},{},{:.1f},{}\n",
            measurement.date, measurement.weight.value, measurement.body_fat.value,
            measurement.water.value, measurement.muscle_mass.value, measurement.energy.value,
            measurement.bmi, measurement.rfm, measurement.body_fat_v2.value,
            measurement.water_v2.value, measurement.muscle_mass_v2.value,
            measurement.energy_v2.value, measurement.ecw_v2.value, measurement.formula_version);
    }
    return csv;
}

auto DataExporter::to_json(const std::vector<Measurement>& data) -> std::string {
    nlohmann::json json_array = nlohmann::json::array();
    for (const auto& measurement : data) {
        json_array.push_back({{"date", std::format("{:%F}", measurement.date)},
                              {"weight_kg", measurement.weight.value},
                              {"body_fat_pct", measurement.body_fat.value},
                              {"water_pct", measurement.water.value},
                              {"muscle_mass_pct", measurement.muscle_mass.value},
                              {"energy_kcal", measurement.energy.value},
                              {"bmi", measurement.bmi},
                              {"rfm", measurement.rfm},
                              {"body_fat_v2_pct", measurement.body_fat_v2.value},
                              {"water_v2_pct", measurement.water_v2.value},
                              {"muscle_mass_v2_pct", measurement.muscle_mass_v2.value},
                              {"energy_v2_kcal", measurement.energy_v2.value},
                              {"ecw_v2_pct", measurement.ecw_v2.value},
                              {"formula_version", measurement.formula_version}});
    }
    static constexpr int json_indent = 4;
    return json_array.dump(json_indent);
}

}  // namespace libscale
