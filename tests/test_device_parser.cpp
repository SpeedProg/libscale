#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "libscale/device.hpp"
#include "libscale/types.hpp"

using namespace libscale;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
namespace {

constexpr size_t user_packet_size = 16;
constexpr size_t measurement_packet_size = 10;
constexpr size_t name_len = 10;

auto build_user_packet(const std::string& name, uint8_t height, uint8_t sex_activity,
                       uint16_t birth_year, uint8_t birth_month_raw, uint8_t birth_day_raw)
    -> std::vector<uint8_t> {
    std::vector<uint8_t> packet(user_packet_size, 0);
    size_t const copied = std::min(name.size(), name_len);
    std::ranges::copy_n(name.begin(), copied, packet.begin());
    for (size_t i = copied; i < name_len; ++i) {
        packet[i] = ' ';
    }
    packet[10] = height;
    packet[11] = sex_activity;
    packet[12] = static_cast<uint8_t>(birth_year >> 8);
    packet[13] = static_cast<uint8_t>(birth_year & 0xFF);
    packet[14] = birth_month_raw;
    packet[15] = birth_day_raw;
    return packet;
}

auto build_measurement_packet(uint16_t year, uint8_t month_raw, uint8_t day_raw, uint16_t imp5k,
                              uint16_t imp50k, uint16_t weight_raw) -> std::vector<uint8_t> {
    std::vector<uint8_t> packet(measurement_packet_size, 0);
    packet[0] = static_cast<uint8_t>(year >> 8);
    packet[1] = static_cast<uint8_t>(year & 0xFF);
    packet[2] = month_raw;
    packet[3] = day_raw;
    packet[4] = static_cast<uint8_t>(imp5k >> 8);
    packet[5] = static_cast<uint8_t>(imp5k & 0xFF);
    packet[6] = static_cast<uint8_t>(imp50k >> 8);
    packet[7] = static_cast<uint8_t>(imp50k & 0xFF);
    packet[8] = static_cast<uint8_t>(weight_raw >> 8);
    packet[9] = static_cast<uint8_t>(weight_raw & 0xFF);
    return packet;
}

void append_packet(std::vector<uint8_t>& destination, const std::vector<uint8_t>& packet) {
    destination.insert(destination.end(), packet.begin(), packet.end());
}

}  // namespace

TEST(DeviceParserTest /*unused*/, ParsesUserPacketWithZeroIndexedDate) {
    // Female (bit 7 set), raw activity 2 -> domain ActivityLevel::HomeGarden (3).
    auto const packet = build_user_packet("Alice", 170, 0x82, 1990, 4, 14);

    auto const users = UsbScaleDevice::parse_users(packet);

    ASSERT_EQ(users.size(), 1);
    EXPECT_EQ(users[0].name, "Alice");
    EXPECT_EQ(users[0].height.value, 170);
    EXPECT_EQ(users[0].gender, Gender::Female);
    EXPECT_EQ(users[0].activity_level, ActivityLevel::HomeGarden);
    EXPECT_EQ(users[0].birthday, std::chrono::year(1990) / 5 / 15);
    EXPECT_EQ(users[0].slot_id, 1);
    EXPECT_EQ(users[0].device_user_id, "slot_1");
}

TEST(DeviceParserTest /*unused*/, TrimsNamePaddingAndNulBytes) {
    auto packet = build_user_packet("Bob", 180, 0x00, 1985, 0, 0);
    std::fill(packet.begin() + 3, packet.begin() + static_cast<std::ptrdiff_t>(name_len), '\0');

    auto const users = UsbScaleDevice::parse_users(packet);

    ASSERT_EQ(users.size(), 1);
    EXPECT_EQ(users[0].name, "Bob");
    EXPECT_EQ(users[0].gender, Gender::Male);
}

TEST(DeviceParserTest /*unused*/, ClampsOutOfRangeActivityLevels) {
    // Raw activity bits 7 (+1 = 8) are invalid and must be clamped to HighLevel.
    auto const packet = build_user_packet("Eve", 165, 0x07, 1992, 0, 0);

    auto const users = UsbScaleDevice::parse_users(packet);

    ASSERT_EQ(users.size(), 1);
    EXPECT_EQ(users[0].activity_level, ActivityLevel::HighLevel);
}

TEST(DeviceParserTest /*unused*/, AssignsSequentialSlots) {
    std::vector<uint8_t> data;
    append_packet(data, build_user_packet("One", 170, 0x00, 1990, 0, 0));
    append_packet(data, build_user_packet("Two", 175, 0x82, 1991, 1, 1));

    auto const users = UsbScaleDevice::parse_users(data);

    ASSERT_EQ(users.size(), 2);
    EXPECT_EQ(users[0].device_user_id, "slot_1");
    EXPECT_EQ(users[1].device_user_id, "slot_2");
    EXPECT_EQ(users[1].slot_id, 2);
}

TEST(DeviceParserTest /*unused*/, IgnoresTruncatedUserData) {
    auto packet = build_user_packet("Alice", 170, 0x82, 1990, 4, 14);
    packet.pop_back();

    EXPECT_TRUE(UsbScaleDevice::parse_users(packet).empty());
    EXPECT_TRUE(UsbScaleDevice::parse_users({}).empty());
}

TEST(DeviceParserTest /*unused*/, ParsesMeasurementPacket) {
    auto const packet = build_measurement_packet(2026, 4, 21, 500, 450, 650);

    auto const measurements = UsbScaleDevice::parse_measurements(packet);

    ASSERT_EQ(measurements.size(), 1);
    EXPECT_EQ(measurements[0].year, 2026);
    EXPECT_EQ(measurements[0].month, 5);
    EXPECT_EQ(measurements[0].day, 22);
    EXPECT_EQ(measurements[0].imp5k, 500);
    EXPECT_EQ(measurements[0].imp50k, 450);
    EXPECT_EQ(measurements[0].weight_raw, 650);
    EXPECT_NEAR(measurements[0].weight().value, 65.0, 0.001);
}

TEST(DeviceParserTest /*unused*/, IgnoresTruncatedMeasurementData) {
    auto packet = build_measurement_packet(2026, 4, 21, 500, 450, 650);
    packet.pop_back();

    EXPECT_TRUE(UsbScaleDevice::parse_measurements(packet).empty());
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
