#pragma once

#include <array>
#include <stdexcept>
#include <string_view>

namespace kns {

enum class DeviceType {
    Unknown, Computer, Router, Switch, AccessPoint, Server, Phone, Printer, IoT, NetworkSegment
};

inline constexpr std::array<std::string_view, 10> deviceTypeNames{
    "unknown", "computer", "router", "switch", "access_point", "server", "phone", "printer", "iot", "network_segment"
};

constexpr std::string_view toString(DeviceType type) noexcept
{
    const auto index = static_cast<std::size_t>(type);
    return index < deviceTypeNames.size() ? deviceTypeNames[index] : "unknown";
}

inline DeviceType deviceTypeFromString(std::string_view value)
{
    for (std::size_t index = 0; index < deviceTypeNames.size(); ++index) {
        if (deviceTypeNames[index] == value) return static_cast<DeviceType>(index);
    }
    throw std::invalid_argument("Unknown device type");
}

} // namespace kns
