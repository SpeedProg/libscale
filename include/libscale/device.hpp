#pragma once
#include <libusb.h>

#ifdef ERROR
// windows.h (included by libusb.h on Windows) defines ERROR as 0, which conflicts with
// Gtk::MessageType::ERROR.
#undef ERROR
#endif

#include <sigc++/sigc++.h>

#include <mutex>
#include <span>
#include <thread>
#include <vector>

#include "types.hpp"

namespace libscale {

/**
 * @brief Abstract interface representing scale hardware communication.
 * Enables mock-based testing and decouple scale logic from USB protocols.
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
     * @brief Signal emitted whenever the scale device is plugged in or arrives.
     * @return sigc::signal reference.
     */
    virtual auto signal_device_arrived() -> sigc::signal<void()> = 0;

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
     * @brief Overrides signal_device_arrived to notify when a USB scale is connected.
     * @return sigc::signal reference.
     */
    auto signal_device_arrived() -> sigc::signal<void()> final {
        return m_signal_arrived;
    }

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
     * @brief Custom RAII deleter for managing libusb context and device handles.
     */
    struct LibUsbDeleter {
        /**
         * @brief Exits the libusb context.
         * @param ctx Pointer to the libusb context.
         */
        void operator()(libusb_context* ctx) const {
            if (ctx != nullptr) {
                libusb_exit(ctx);
            }
        }
        /**
         * @brief Closes the libusb device handle.
         * @param handle Pointer to the device handle.
         */
        void operator()(libusb_device_handle* handle) const {
            if (handle != nullptr) {
                libusb_close(handle);
            }
        }
    };

    /**
     * @brief RAII wrapper for registering/deregistering hotplug callback handles in libusb.
     */
    class HotplugHandle {
       public:
        /**
         * @brief Default constructor.
         */
        HotplugHandle() = default;

        /**
         * @brief Constructs a HotplugHandle representing an active registered hotplug callback.
         * @param ctx Libusb context.
         * @param handle Callback handle.
         */
        HotplugHandle(libusb_context* ctx, libusb_hotplug_callback_handle handle) noexcept;

        /**
         * @brief Destructor. Deregisters the hotplug callback from libusb context.
         */
        ~HotplugHandle();

        HotplugHandle(const HotplugHandle&) = delete;
        auto operator=(const HotplugHandle&) -> HotplugHandle& = delete;

        /**
         * @brief Move constructor.
         * @param other The handle to move from.
         */
        HotplugHandle(HotplugHandle&& other) noexcept;

        /**
         * @brief Move assignment operator.
         * @param other The handle to move from.
         * @return Reference to this HotplugHandle.
         */
        auto operator=(HotplugHandle&& other) noexcept -> HotplugHandle&;

        /**
         * @brief Deregisters the hotplug callback if registered.
         */
        void reset() noexcept;

       private:
        void reset_internal() noexcept;

        libusb_context* m_ctx = nullptr;
        libusb_hotplug_callback_handle m_handle = 0;
        mutable std::mutex m_mutex;
    };

    void cleanup_internal();

    void send_command(const std::vector<uint8_t>& cmd);
    auto read_response(int length) -> std::vector<uint8_t>;
    static auto LIBUSB_CALL hotplug_callback(libusb_context* ctx, libusb_device* dev,
                                             libusb_hotplug_event event, void* user_data) -> int;

    std::unique_ptr<libusb_context, LibUsbDeleter> m_ctx;
    std::unique_ptr<libusb_device_handle, LibUsbDeleter> m_handle;
    HotplugHandle m_hotplug;
    sigc::signal<void()> m_signal_arrived;
    std::jthread m_event_thread;
};

inline UsbScaleDevice::HotplugHandle::HotplugHandle(libusb_context* ctx,
                                                    libusb_hotplug_callback_handle handle) noexcept
    : m_ctx(ctx), m_handle(handle) {}

inline UsbScaleDevice::HotplugHandle::~HotplugHandle() {
    reset();
}

inline UsbScaleDevice::HotplugHandle::HotplugHandle(HotplugHandle&& other) noexcept {
    std::lock_guard const lock(other.m_mutex);
    m_ctx = other.m_ctx;       // NOLINT(cppcoreguidelines-prefer-member-initializer)
    m_handle = other.m_handle; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    other.m_ctx = nullptr;
    other.m_handle = 0;
}

inline auto UsbScaleDevice::HotplugHandle::operator=(HotplugHandle&& other) noexcept
    -> UsbScaleDevice::HotplugHandle& {
    if (this != &other) {
        std::lock(m_mutex, other.m_mutex);
        std::lock_guard const lock_this(m_mutex, std::adopt_lock);
        std::lock_guard const lock_other(other.m_mutex, std::adopt_lock);

        reset_internal();
        m_ctx = other.m_ctx;
        m_handle = other.m_handle;
        other.m_ctx = nullptr;
        other.m_handle = 0;
    }
    return *this;
}

inline void UsbScaleDevice::HotplugHandle::reset() noexcept {
    std::lock_guard const lock(m_mutex);
    reset_internal();
}

inline void UsbScaleDevice::HotplugHandle::reset_internal() noexcept {
    if ((m_ctx != nullptr) && m_handle != 0) {
        libusb_hotplug_deregister_callback(m_ctx, m_handle);
    }
    m_ctx = nullptr;
    m_handle = 0;
}

}  // namespace libscale
