
#include "libscale/exporter.hpp"

#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "libscale/types.hpp"

namespace libscale {

auto DataExporter::to_csv(const std::vector<Measurement>& data) -> std::string {
    std::string csv = "Date,Weight(kg),BodyFat(%),Water(%),Muscle(%),Energy(kcal),BMI,Version\n";
    for (const auto& measurement : data) {
        csv += std::format("{:%F},{:.1f},{:.1f},{:.1f},{:.1f},{},{:.1f},{}\n", measurement.date,
                           measurement.weight.value, measurement.body_fat.value,
                           measurement.water.value, measurement.muscle_mass.value,
                           measurement.energy.value, measurement.bmi, measurement.formula_version);
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
                              {"formula_version", measurement.formula_version}});
    }
    static constexpr int json_indent = 4;
    return json_array.dump(json_indent);
}

}  // namespace libscale
