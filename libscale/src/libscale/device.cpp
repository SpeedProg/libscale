#include <format>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "libscale/units.hpp"
#ifdef _WIN32
#include <winsock2.h>  // Conditionally compiled header providing timeval struct on Windows.
#else
#include <sys/time.h>  // Conditionally compiled header providing timeval struct on POSIX.
#endif

#include <libusb.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "libscale/device.hpp"
#include "libscale/types.hpp"

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): Subscripting is safe
// since packet offsets are bounds-checked in loops.
namespace libscale {

namespace {
constexpr uint16_t vid = 0x222D;
constexpr uint16_t pid = 0x0001;
constexpr size_t slot_prefix_len = 5;

constexpr uint8_t cmd_user_data = 0x22;
constexpr uint8_t cmd_measurement_data = 0x33;
constexpr uint8_t subcmd_get_size = 0x00;
constexpr uint8_t subcmd_get_data = 0x01;

constexpr int measurement_packet_size = 10;
constexpr int user_packet_size = 16;
constexpr int bits_per_byte = 8;

constexpr int usb_timeout_ms = 1000;
constexpr int polling_interval_ms = 500;
constexpr uint8_t endpoint_out = 0x01;
constexpr uint8_t endpoint_in = 0x81;

// User packet offsets
constexpr int off_user_name = 0;
constexpr int off_user_height = 10;
constexpr int off_user_sex_activity = 11;
constexpr int off_user_bday_year = 12;
constexpr int off_user_bday_month = 14;
constexpr int off_user_bday_day = 15;

// Measurement packet offsets
constexpr int off_meas_year = 0;
constexpr int off_meas_month = 2;
constexpr int off_meas_day = 3;
constexpr int off_meas_imp5k = 4;
constexpr int off_meas_imp50k = 6;
constexpr int off_meas_weight = 8;

constexpr int name_len = 10;
constexpr uint8_t gender_bit = 7;
constexpr uint8_t activity_mask = 0x07;
// Helper to parse Big-Endian integers from a byte span
template <typename T>
auto from_be(std::span<const uint8_t> data, size_t offset) -> T {
    if (offset + sizeof(T) > data.size()) {
        return 0;
    }
    T val = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        val = static_cast<T>((static_cast<unsigned int>(val) << bits_per_byte) | data[offset + i]);
    }
    return val;
}

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
    HotplugHandle(libusb_context* ctx, libusb_hotplug_callback_handle handle) noexcept
        : m_ctx(ctx), m_handle(handle) {}

    /**
     * @brief Destructor. Deregisters the hotplug callback from libusb context.
     */
    ~HotplugHandle() {
        reset();
    }

    HotplugHandle(const HotplugHandle&) = delete;
    auto operator=(const HotplugHandle&) -> HotplugHandle& = delete;

    /**
     * @brief Move constructor.
     * @param other The handle to move from.
     */
    HotplugHandle(HotplugHandle&& other) noexcept {
        std::scoped_lock const lock(other.m_mutex);
        m_ctx = other.m_ctx;        // NOLINT(cppcoreguidelines-prefer-member-initializer)
        m_handle = other.m_handle;  // NOLINT(cppcoreguidelines-prefer-member-initializer)
        other.m_ctx = nullptr;
        other.m_handle = 0;
    }

    /**
     * @brief Move assignment operator.
     * @param other The handle to move from.
     * @return Reference to this HotplugHandle.
     */
    auto operator=(HotplugHandle&& other) noexcept -> HotplugHandle& {
        if (this != &other) {
            std::scoped_lock const lock(m_mutex, other.m_mutex);

            reset_internal();
            m_ctx = other.m_ctx;
            m_handle = other.m_handle;
            other.m_ctx = nullptr;
            other.m_handle = 0;
        }
        return *this;
    }

    /**
     * @brief Deregisters the hotplug callback if registered.
     */
    void reset() noexcept {
        std::scoped_lock const lock(m_mutex);
        reset_internal();
    }

   private:
    void reset_internal() noexcept {
        if ((m_ctx != nullptr) && m_handle != 0) {
            libusb_hotplug_deregister_callback(m_ctx, m_handle);
        }
        m_ctx = nullptr;
        m_handle = 0;
    }

    libusb_context* m_ctx = nullptr;
    libusb_hotplug_callback_handle m_handle = 0;
    mutable std::mutex m_mutex;
};
}  // namespace

class UsbScaleDevice::Impl {
   public:
    Impl() {
        libusb_context* ctx_raw = nullptr;
        int ret = libusb_init(&ctx_raw);
        if (ret != LIBUSB_SUCCESS) {
            throw std::runtime_error(
                std::format("Failed to initialize libusb: {}", libusb_error_name(ret)));
        }
        m_ctx.reset(ctx_raw);

        if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG) != 0) {
            libusb_hotplug_callback_handle handle = 0;
            ret = libusb_hotplug_register_callback(
                m_ctx.get(), LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, LIBUSB_HOTPLUG_ENUMERATE, vid,
                pid, LIBUSB_HOTPLUG_MATCH_ANY, &Impl::hotplug_callback, this, &handle);

            if (ret == LIBUSB_SUCCESS) {
                m_hotplug = HotplugHandle(m_ctx.get(), handle);
            }

            // Start polling thread with C++20 stop_token
            m_event_thread = std::jthread([this](const std::stop_token& stop_token) -> void {
                while (!stop_token.stop_requested()) {
                    struct timeval time_val = {.tv_sec = 1, .tv_usec = 0};
                    int const ret_evt =
                        libusb_handle_events_timeout_completed(m_ctx.get(), &time_val, nullptr);
                    if (ret_evt < 0 && ret_evt != LIBUSB_ERROR_INTERRUPTED) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(polling_interval_ms));
                    }
                }
            });
        }
    }

    ~Impl() {
        disconnect();
    }

    Impl(const Impl&) = delete;
    auto operator=(const Impl&) -> Impl& = delete;
    Impl(Impl&&) = delete;
    auto operator=(Impl&&) -> Impl& = delete;

    auto connect() -> bool {
        if (m_handle) {
            return true;
        }

        libusb_device_handle* handle_raw = libusb_open_device_with_vid_pid(m_ctx.get(), vid, pid);
        if (handle_raw == nullptr) {
            return false;
        }
        m_handle.reset(handle_raw);

#ifdef __linux__
        // Detach kernel driver if necessary (Linux)
        if (libusb_kernel_driver_active(m_handle.get(), 0) == 1) {
            libusb_detach_kernel_driver(m_handle.get(), 0);
        }
#endif

        if (libusb_claim_interface(m_handle.get(), 0) < 0) {
            disconnect();
            return false;
        }

        return true;
    }

    void disconnect() {
        if (m_handle) {
            libusb_release_interface(m_handle.get(), 0);
            m_handle.reset();
        }
    }

    [[nodiscard]] auto is_connected() const -> bool {
        return m_handle != nullptr;
    }

    void set_arrival_callback(std::function<void()> callback) {
        std::scoped_lock const lock(m_callback_mutex);
        m_arrival_callback = std::move(callback);
    }

    auto read_users() -> std::vector<UserProfile> {
        if (m_handle == nullptr) {
            throw std::runtime_error("Scale is not connected.");
        }

        // Get size
        send_command({cmd_user_data, subcmd_get_size, 0x00, 0x00});
        auto resp = read_response(4);
        if (resp.size() < 3) {
            throw std::runtime_error("Unexpected user list response from scale.");
        }

        auto const total_bytes =
            static_cast<uint16_t>((static_cast<uint16_t>(resp[1]) << bits_per_byte) | resp[2]);
        if (total_bytes == 0) {
            return {};
        }

        // Get data
        send_command({cmd_user_data, subcmd_get_data, resp[1], resp[2]});
        auto data = read_response(total_bytes);
        return UsbScaleDevice::parse_users(data);
    }

    auto read_measurements(const std::string& device_user_id) -> std::vector<RawMeasurement> {
        if (m_handle == nullptr) {
            throw std::runtime_error("Scale is not connected.");
        }

        int slot_id = 0;
        if (device_user_id.starts_with("slot_")) {
            try {
                slot_id = std::stoi(device_user_id.substr(slot_prefix_len));
            } catch (...) {
                slot_id = 0;
            }
        }

        if (slot_id <= 0) {
            return {};
        }

        // Get size
        send_command({cmd_measurement_data, subcmd_get_size, 0x00, static_cast<uint8_t>(slot_id)});
        auto resp = read_response(4);
        if (resp.size() < 3) {
            throw std::runtime_error("Unexpected measurement list response from scale.");
        }

        auto const total_bytes =
            static_cast<uint16_t>((static_cast<uint16_t>(resp[1]) << bits_per_byte) | resp[2]);
        if (total_bytes == 0) {
            return {};
        }

        // Get data
        send_command({cmd_measurement_data, subcmd_get_data, resp[2], resp[1],
                      static_cast<uint8_t>(slot_id)});
        auto data = read_response(total_bytes);
        return UsbScaleDevice::parse_measurements(data);
    }

   private:
    static auto LIBUSB_CALL hotplug_callback(libusb_context* /*ctx*/, libusb_device* /*dev*/,
                                             libusb_hotplug_event /*event*/, void* user_data)
        -> int {
        auto* self = static_cast<Impl*>(user_data);
        self->notify_arrived();
        return 0;
    }

    void notify_arrived() {
        // Invoking while holding the lock makes set_arrival_callback(nullptr) a synchronisation
        // point: once it returns, no callback touching a destroyed consumer can be in flight.
        std::scoped_lock const lock(m_callback_mutex);
        if (m_arrival_callback) {
            m_arrival_callback();
        }
    }

    void send_command(std::vector<uint8_t> cmd) {
        int transferred = 0;
        int const result =
            libusb_bulk_transfer(m_handle.get(), endpoint_out, cmd.data(),
                                 static_cast<int>(cmd.size()), &transferred, usb_timeout_ms);
        if (result != LIBUSB_SUCCESS) {
            throw std::runtime_error(
                std::format("USB command transfer failed: {}", libusb_error_name(result)));
        }
        if (static_cast<size_t>(transferred) != cmd.size()) {
            throw std::runtime_error("USB command transfer was truncated.");
        }
    }

    auto read_response(int length) -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer(length);
        int transferred = 0;
        int const result = libusb_bulk_transfer(m_handle.get(), endpoint_in, buffer.data(), length,
                                                &transferred, usb_timeout_ms);
        if (result != LIBUSB_SUCCESS) {
            throw std::runtime_error(
                std::format("USB response transfer failed: {}", libusb_error_name(result)));
        }
        buffer.resize(static_cast<size_t>(transferred));
        return buffer;
    }

    std::unique_ptr<libusb_context, LibUsbDeleter> m_ctx;
    std::unique_ptr<libusb_device_handle, LibUsbDeleter> m_handle;
    HotplugHandle m_hotplug;
    std::function<void()> m_arrival_callback;
    mutable std::mutex m_callback_mutex;
    std::jthread m_event_thread;
};

UsbScaleDevice::UsbScaleDevice() : m_impl(std::make_unique<Impl>()) {}

UsbScaleDevice::~UsbScaleDevice() = default;

auto UsbScaleDevice::is_connected() const -> bool {
    return m_impl->is_connected();
}

auto UsbScaleDevice::connect() -> bool {
    return m_impl->connect();
}

void UsbScaleDevice::disconnect() {
    m_impl->disconnect();
}

auto UsbScaleDevice::read_users() -> std::vector<UserProfile> {
    return m_impl->read_users();
}

auto UsbScaleDevice::read_measurements(const std::string& device_user_id)
    -> std::vector<RawMeasurement> {
    return m_impl->read_measurements(device_user_id);
}

void UsbScaleDevice::set_device_arrived_callback(std::function<void()> callback) {
    m_impl->set_arrival_callback(std::move(callback));
}

auto UsbScaleDevice::parse_users(std::span<const uint8_t> data) -> std::vector<UserProfile> {
    std::vector<UserProfile> users{};
    for (size_t i = 0; i + (user_packet_size - 1) < data.size(); i += user_packet_size) {
        UserProfile user;
        user.slot_id = static_cast<int>(i / user_packet_size) + 1;
        user.device_user_id = "slot_" + std::to_string(user.slot_id);

        // Name (10 chars) - use memcpy-safe construction via std::string from iterators
        // Casting raw byte array containing name characters to const char* to construct
        // std::string. NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        user.name = std::string(reinterpret_cast<const char*>(&data[i + off_user_name]), name_len);
        // Robust trimming
        user.name.erase(std::ranges::find_if(user.name.rbegin(), user.name.rend(),
                                             [](unsigned char chr) -> bool {
                                                 return (std::isspace(chr) == 0) && chr != '\0';
                                             })
                            .base(),
                        user.name.end());

        user.height = units::Cm{data[i + off_user_height]};

        // Telemetry Protocol Design: The sex/gender and physical activity level are tightly packed
        // into a single byte `sex_activity` to minimize packet transmission size.
        // - Bit 7 (MSB): Gender flag (0 = Male, 1 = Female).
        // - Bits 0-2 (3 LSB): Activity level (values 0-4 representing Scarcely to Extremely
        // active). We extract the gender by shifting the gender_bit (7) and extract the activity
        // level using the 3-bit activity mask (0x07), adding 1 to translate the raw 0-indexed scale
        // storage value to our 1-indexed domain model ActivityLevel. Values outside the domain
        // range are clamped so corrupt packets cannot produce invalid enum values.
        uint8_t const sex_activity = data[i + off_user_sex_activity];
        user.gender = ((sex_activity >> gender_bit) != 0) ? Gender::Female : Gender::Male;
        int const encoded_activity = (sex_activity & activity_mask) + 1;
        constexpr int min_activity = static_cast<int>(ActivityLevel::Scarcely);
        constexpr int max_activity = static_cast<int>(ActivityLevel::HighLevel);
        user.activity_level =
            static_cast<ActivityLevel>(std::clamp(encoded_activity, min_activity, max_activity));

        // Hardware Date Representation: Birth year is transmitted as a 16-bit Big-Endian integer.
        // Month and day fields are stored in the scale's internal memory as 0-indexed values
        // (0 = January, 0 = 1st day of the month) to save space or ease calendar calculations.
        // We apply a +1 offset here to construct valid standard Gregorian calendar date instances.
        auto const year = from_be<uint16_t>(data, i + off_user_bday_year);
        auto const month = static_cast<uint8_t>(data[i + off_user_bday_month] + 1);
        auto const day = static_cast<uint8_t>(data[i + off_user_bday_day] + 1);

        user.birthday = std::chrono::year(year) / month / day;

        users.push_back(user);
    }
    return users;
}

auto UsbScaleDevice::parse_measurements(std::span<const uint8_t> data)
    -> std::vector<RawMeasurement> {
    std::vector<RawMeasurement> measurements{};
    for (size_t i = 0; i + (measurement_packet_size - 1) < data.size();
         i += measurement_packet_size) {
        RawMeasurement measurement;
        // Hardware Protocol Alignment: Measurement timestamps are similarly offset by the scale
        // logic. The month and day are 0-indexed in the USB byte stream, requiring a +1 correction
        // to represent a standard Gregorian calendar date.
        measurement.year = from_be<uint16_t>(data, i + off_meas_year);
        measurement.month = static_cast<uint8_t>(data[i + off_meas_month] + 1);
        measurement.day = static_cast<uint8_t>(data[i + off_meas_day] + 1);
        measurement.imp5k = from_be<uint16_t>(data, i + off_meas_imp5k);
        measurement.imp50k = from_be<uint16_t>(data, i + off_meas_imp50k);
        measurement.weight_raw = from_be<uint16_t>(data, i + off_meas_weight);
        measurements.push_back(measurement);
    }
    return measurements;
}

auto UsbScaleDevice::get_device_key() const -> std::string_view {
    return "usb:222d:0001";
}

auto UsbScaleDevice::get_device_name() const -> std::string_view {
    return "Soehnle Comfort Select";
}

auto UsbScaleDevice::get_impedance_pathway() const -> ImpedancePathway {
    return ImpedancePathway::FootToFoot;
}

}  // namespace libscale
// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
