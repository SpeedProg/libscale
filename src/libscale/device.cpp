#include "libscale/device.hpp"

#if defined(_WIN32)
#include <winsock2.h>  // NOLINT(misc-include-cleaner): Conditionally compiled header providing timeval struct on Windows.
#else
#include <sys/time.h>  // NOLINT(misc-include-cleaner): Conditionally compiled header providing timeval struct on POSIX.
#endif
#include <libusb.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "libscale/types.hpp"

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
        val = static_cast<T>((val << bits_per_byte) | data[offset + i]);
    }
    return val;
}
}  // namespace

UsbScaleDevice::UsbScaleDevice() {
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
            m_ctx.get(), LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, LIBUSB_HOTPLUG_ENUMERATE, vid, pid,
            LIBUSB_HOTPLUG_MATCH_ANY, &UsbScaleDevice::hotplug_callback, this, &handle);

        if (ret == LIBUSB_SUCCESS) {
            m_hotplug = HotplugHandle(m_ctx.get(), handle);
        }

        // Start polling thread with C++20 stop_token
        m_event_thread = std::jthread([this](const std::stop_token& stop_token) {
            while (!stop_token.stop_requested()) {
                // timeval struct definition is provided by conditionally compiled platform headers.
                // NOLINTNEXTLINE(misc-include-cleaner)
                struct timeval time_val = {1, 0};
                int const ret_evt =
                    libusb_handle_events_timeout_completed(m_ctx.get(), &time_val, nullptr);
                if (ret_evt < 0 && ret_evt != LIBUSB_ERROR_INTERRUPTED) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(polling_interval_ms));
                }
            }
        });
    }
}

auto LIBUSB_CALL UsbScaleDevice::hotplug_callback(libusb_context* /*ctx*/, libusb_device* /*dev*/,
                                                  libusb_hotplug_event /*event*/,
                                                  void* user_data) -> int {
    auto* self = static_cast<UsbScaleDevice*>(user_data);
    self->m_signal_arrived.emit();
    return 0;
}

UsbScaleDevice::~UsbScaleDevice() {
    cleanup_internal();
}

void UsbScaleDevice::cleanup_internal() {
    if (m_handle) {
        libusb_release_interface(m_handle.get(), 0);
        m_handle.reset();
    }
}

auto UsbScaleDevice::connect() -> bool {
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
        cleanup_internal();
        return false;
    }

    return true;
}

void UsbScaleDevice::disconnect() {
    cleanup_internal();
}

auto UsbScaleDevice::is_connected() const -> bool {
    return m_handle != nullptr;
}

void UsbScaleDevice::send_command(const std::vector<uint8_t>& cmd) {
    int transferred = 0;
    std::vector<uint8_t> mutable_cmd = cmd;
    libusb_bulk_transfer(m_handle.get(), endpoint_out, mutable_cmd.data(),
                         static_cast<int>(mutable_cmd.size()), &transferred, usb_timeout_ms);
}

auto UsbScaleDevice::read_response(int length) -> std::vector<uint8_t> {
    std::vector<uint8_t> buffer(length);
    int transferred = 0;
    libusb_bulk_transfer(m_handle.get(), endpoint_in, buffer.data(), length, &transferred,
                         usb_timeout_ms);
    buffer.resize(transferred);
    return buffer;
}

auto UsbScaleDevice::read_users() -> std::vector<UserProfile> {
    if (m_handle == nullptr) {
        return {};
    }

    // Get size
    send_command({cmd_user_data, subcmd_get_size, 0x00, 0x00});
    auto resp = read_response(4);
    if (resp.size() < 3) {
        return {};
    }

    auto total_bytes = static_cast<uint16_t>((resp[1] << bits_per_byte) | resp[2]);
    if (total_bytes == 0) {
        return {};
    }

    // Get data
    send_command({cmd_user_data, subcmd_get_data, resp[1], resp[2]});
    auto data = read_response(total_bytes);
    return parse_users(data);
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
        user.name.erase(
            std::find_if(user.name.rbegin(), user.name.rend(),
                         [](unsigned char chr) { return (std::isspace(chr) == 0) && chr != '\0'; })
                .base(),
            user.name.end());

        user.height = units::Cm{data[i + off_user_height]};

        // Telemetry Protocol Design: The sex/gender and physical activity level are tightly packed
        // into a single byte `sex_activity` to minimize packet transmission size.
        // - Bit 7 (MSB): Gender flag (0 = Male, 1 = Female).
        // - Bits 0-2 (3 LSB): Activity level (values 0-4 representing Scarcely to Extremely
        // active). We extract the gender by shifting the gender_bit (7) and extract the activity
        // level using the 3-bit activity mask (0x07), adding 1 to translate the raw 0-indexed scale
        // storage value to our 1-indexed domain model ActivityLevel.
        uint8_t const sex_activity = data[i + off_user_sex_activity];
        user.gender = ((sex_activity >> gender_bit) != 0) ? Gender::Female : Gender::Male;
        user.activity_level = static_cast<ActivityLevel>((sex_activity & activity_mask) + 1);

        // Hardware Date Representation: Birth year is transmitted as a 16-bit Big-Endian integer.
        // Month and day fields are stored in the scale's internal memory as 0-indexed values
        // (0 = January, 0 = 1st day of the month) to save space or ease calendar calculations.
        // We apply a +1 offset here to construct valid standard Gregorian calendar date instances.
        auto year = from_be<uint16_t>(data, i + off_user_bday_year);
        auto month = static_cast<uint8_t>(data[i + off_user_bday_month] + 1);
        auto day = static_cast<uint8_t>(data[i + off_user_bday_day] + 1);

        user.birthday = std::chrono::year(year) / month / day;

        users.push_back(user);
    }
    return users;
}

auto UsbScaleDevice::read_measurements(const std::string& device_user_id)
    -> std::vector<RawMeasurement> {
    if (m_handle == nullptr) {
        return {};
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
        return {};
    }

    auto total_bytes = static_cast<uint16_t>((resp[1] << bits_per_byte) | resp[2]);
    if (total_bytes == 0) {
        return {};
    }

    // Get data
    send_command(
        {cmd_measurement_data, subcmd_get_data, resp[2], resp[1], static_cast<uint8_t>(slot_id)});
    auto data = read_response(total_bytes);
    return parse_measurements(data);
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

}  // namespace libscale
