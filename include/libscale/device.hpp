#pragma once
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace libscale {

/**
 * @brief Abstract interface representing scale hardware communication.
 * Enables mock-based testing and decouples scale logic from USB protocols.
 */
class IScaleDevice {
   public:
    /**
     * @brief Default constructor.
     */
    IScaleDevice() = default;

    /**
     * @brief Virtual destructor to ensure proper cleanup of hardware resources.
     */
    virtual ~IScaleDevice() = default;

    IScaleDevice(const IScaleDevice&) = delete;
    auto operator=(const IScaleDevice&) -> IScaleDevice& = delete;
    IScaleDevice(IScaleDevice&&) = delete;
    auto operator=(IScaleDevice&&) -> IScaleDevice& = delete;

    /**
     * @brief Establishes a connection to the scale device.
     * @return True if connected successfully, false otherwise.
     */
    virtual auto connect() -> bool = 0;

    /**
     * @brief Closes the connection to the scale device.
     */
    virtual void disconnect() = 0;

    /**
     * @brief Checks if the scale device is currently connected.
     * @return True if connected, false otherwise.
     */
    [[nodiscard]] virtual auto is_connected() const -> bool = 0;

    /**
     * @brief Reads all user profiles registered on the physical scale.
     * @return List of user profiles retrieved from hardware.
     */
    virtual auto read_users() -> std::vector<UserProfile> = 0;

    /**
     * @brief Reads all raw measurements for a specific user from the scale hardware memory.
     * @param device_user_id The scale-specific user ID string.
     * @return List of raw measurements.
     */
    virtual auto read_measurements(const std::string& device_user_id)
        -> std::vector<RawMeasurement> = 0;

    /**
     * @brief Registers the callback invoked whenever the scale device is plugged in or arrives.
     * The callback runs on a background USB event thread, so consumers that touch UI state must
     * marshal it to their own main loop.
     * @param callback Callback to invoke. Pass an empty std::function to detach.
     */
    virtual void set_device_arrived_callback(std::function<void()> callback) = 0;

    /**
     * @brief Gets the hardware-specific driver key.
     * @return The hardware key string.
     */
    [[nodiscard]] virtual auto get_device_key() const -> std::string_view = 0;

    /**
     * @brief Gets the human-readable name of the device.
     * @return The device name.
     */
    [[nodiscard]] virtual auto get_device_name() const -> std::string_view = 0;

    /**
     * @brief Gets the biometric impedance pathway used by the device.
     * @return The impedance pathway enum.
     */
    [[nodiscard]] virtual auto get_impedance_pathway() const -> ImpedancePathway = 0;
};

/**
 * @brief USB-specific implementation of the scale device using libusb.
 * The libusb context, device handle and hotplug event loop are confined to the implementation
 * file so consumers of this header do not need libusb headers.
 */
class UsbScaleDevice final : public IScaleDevice {
   public:
    /**
     * @brief Constructs a UsbScaleDevice and initializes libusb and hotplug monitoring.
     */
    UsbScaleDevice();

    /**
     * @brief Destructor. Closes active handles and terminates the event-handling thread.
     */
    ~UsbScaleDevice() override;

    UsbScaleDevice(const UsbScaleDevice&) = delete;
    auto operator=(const UsbScaleDevice&) -> UsbScaleDevice& = delete;
    UsbScaleDevice(UsbScaleDevice&&) = delete;
    auto operator=(UsbScaleDevice&&) -> UsbScaleDevice& = delete;

    /**
     * @brief Checks if the USB scale is currently connected.
     * @return True if connected, false otherwise.
     */
    [[nodiscard]] auto is_connected() const -> bool final;

    /**
     * @brief Establishes connection to the USB scale by claiming the interface.
     * @return True if connection succeeded, false otherwise.
     */
    auto connect() -> bool final;

    /**
     * @brief Releases the claimed interface and closes the USB device handle.
     */
    void disconnect() final;

    /**
     * @brief Reads user profiles from the USB device endpoint.
     * @return List of user profiles.
     */
    auto read_users() -> std::vector<UserProfile> final;

    /**
     * @brief Reads raw measurements for a user from the USB device endpoint.
     * @param device_user_id The scale-specific user ID string.
     * @return List of raw measurements.
     */
    auto read_measurements(const std::string& device_user_id) -> std::vector<RawMeasurement> final;

    /**
     * @brief Overrides set_device_arrived_callback to notify when a USB scale is connected.
     * @param callback Callback to invoke. Pass an empty std::function to detach.
     */
    void set_device_arrived_callback(std::function<void()> callback) final;

    /**
     * @brief Parses a binary buffer from the scale containing user profile records.
     * @param data The raw byte span.
     * @return List of parsed user profiles.
     */
    static auto parse_users(std::span<const uint8_t> data) -> std::vector<UserProfile>;

    /**
     * @brief Parses a binary buffer from the scale containing raw measurement records.
     * @param data The raw byte span.
     * @return List of parsed raw measurements.
     */
    static auto parse_measurements(std::span<const uint8_t> data) -> std::vector<RawMeasurement>;

    /**
     * @brief Gets the hardware-specific driver key.
     * @return The hardware key string.
     */
    [[nodiscard]] auto get_device_key() const -> std::string_view final;

    /**
     * @brief Gets the human-readable display and name of the device.
     * @return The device name.
     */
    [[nodiscard]] auto get_device_name() const -> std::string_view final;

    /**
     * @brief Gets the biometric impedance pathway used by the device.
     * @return The impedance pathway enum.
     */
    [[nodiscard]] auto get_impedance_pathway() const -> ImpedancePathway final;

   private:
    /**
     * @brief Implementation hiding the libusb context, device handle and hotplug event loop.
     */
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace libscale
