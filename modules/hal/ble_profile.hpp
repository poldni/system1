/**
 * @file modules/hal/ble_profile.hpp
 * @brief BLE Profile Definitions for Object Tracking System.
 */

#ifndef MODULES_HAL_BLE_PROFILE_HPP
#define MODULES_HAL_BLE_PROFILE_HPP

#include <array>
#include <cstdint>

namespace system1::hal
{

struct BleProfile
{
    // Service UUID: 4a981234-5678-90ab-cdef-1234567890ab
    // Note: UUIDs are typically represented in Big Endian in documentation/strings.
    static constexpr std::array<std::uint8_t, 16> ServiceUuid = {
        0x4a, 0x98, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab,
        0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab
    };

    // Characteristic: Tracking Data (Notify)
    // UUID: 4a981234-5678-90ab-cdef-1234567890ac
    static constexpr std::array<std::uint8_t, 16> TrackingDataCharUuid = {
        0x4a, 0x98, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab,
        0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xac
    };

    // Characteristic: Settings (Read, Write, Notify)
    // UUID: 4a981234-5678-90ab-cdef-1234567890ad
    static constexpr std::array<std::uint8_t, 16> SettingsCharUuid = {
        0x4a, 0x98, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab,
        0xcd, 0xef, 0x12, 0x34, 0x56, 0x78, 0x90, 0xad
    };
};

} // namespace system1::hal

#endif // MODULES_HAL_BLE_PROFILE_HPP
