#pragma once
#include <string>
#include <vector>
#include "types.hpp"

namespace libscale {

/**
 * @brief Utility class to handle serialization and export of measurement data.
 * Adheres to the Single Responsibility Principle by decoupling data format conversion
 * from GUI components or managers.
 */
class DataExporter {
   public:
    /**
     * @brief Serializes a list of computed measurements into standard CSV format.
     * @param data List of measurements to serialize.
     * @return Generated CSV string.
     */
    static auto to_csv(const std::vector<Measurement>& data) -> std::string;

    /**
     * @brief Serializes a list of computed measurements into standard JSON format.
     * @param data List of measurements to serialize.
     * @return Generated JSON string.
     */
    static auto to_json(const std::vector<Measurement>& data) -> std::string;
};

}  // namespace libscale
