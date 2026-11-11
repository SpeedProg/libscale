#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "device.hpp"

namespace libscale {

/**
 * @brief Factory class to dynamically instantiate polymorphic scale data providers.
 * Adheres to Dependency Inversion (DIP) by isolating creation of specific device drivers.
 */
class ScaleProviderFactory {
   public:
    /**
     * @brief Supported scale data provider types.
     */
    enum class ProviderType : std::uint8_t {
        /// Physical USB scale (using libusb)
        UsbComfortSelect
    };

    ScaleProviderFactory() = delete;
    ~ScaleProviderFactory() = delete;

    ScaleProviderFactory(const ScaleProviderFactory&) = delete;
    auto operator=(const ScaleProviderFactory&) -> ScaleProviderFactory& = delete;
    ScaleProviderFactory(ScaleProviderFactory&&) = delete;
    auto operator=(ScaleProviderFactory&&) -> ScaleProviderFactory& = delete;

    /**
     * @brief Instantiates the appropriate scale data provider.
     * @param type Selected provider type.
     * @return Unique pointer to the instantiated IScaleDevice.
     */
    [[nodiscard]] static auto create(ProviderType type) -> std::unique_ptr<IScaleDevice> {
        if (type == ProviderType::UsbComfortSelect) {
            return std::make_unique<UsbScaleDevice>();
        }
        throw std::invalid_argument("Unsupported Scale Provider Type.");
    }
};

}  // namespace libscale
