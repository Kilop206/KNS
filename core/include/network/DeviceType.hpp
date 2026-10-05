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

// KNS simulation profiles, shared by routing, transport and both user interfaces.
struct DeviceCapabilities {
    bool forward;
    bool tcp_client;
    bool tcp_listener;
    bool service_client;
    bool http_server;
    bool dns_server;
};

constexpr DeviceCapabilities deviceCapabilities(DeviceType type) noexcept {
    switch (type) {
        // Untyped graph topologies retain their historical routing/TCP behavior.
        // Application services require selecting an explicit device profile.
        case DeviceType::Unknown:        return {true,  true,  true,  false, false, false};
        case DeviceType::Computer:       return {false, true,  true,  true,  false, false};
        case DeviceType::Router:         return {true,  true,  false, true,  false, true};
        case DeviceType::Switch:         return {true,  false, false, false, false, false};
        case DeviceType::AccessPoint:    return {true,  false, false, false, false, false};
        case DeviceType::Server:         return {false, true,  true,  true,  true,  true};
        case DeviceType::Phone:          return {false, true,  true,  true,  false, false};
        case DeviceType::Printer:        return {false, false, true,  false, true,  false};
        case DeviceType::IoT:            return {false, true,  true,  true,  true,  false};
        case DeviceType::NetworkSegment: return {true,  false, false, false, false, false};
    }
    return {};
}

constexpr std::string_view deviceRoleDescription(DeviceType type) noexcept {
    switch (type) {
        case DeviceType::Unknown: return "Legacy graph node: routing and TCP only. Select a device type to use application services.";
        case DeviceType::Computer: return "Workstation: HTTP/DNS client and TCP endpoint; does not forward transit traffic or host services.";
        case DeviceType::Router: return "Router: forwards transit traffic, supports DNS service and client diagnostics.";
        case DeviceType::Switch: return "Switch: forwards transit traffic; does not originate application traffic or host services.";
        case DeviceType::AccessPoint: return "Access point: relays transit traffic; does not originate application traffic or host services.";
        case DeviceType::Server: return "Server: hosts HTTP/DNS services and acts as a client; does not forward transit traffic.";
        case DeviceType::Phone: return "Phone: HTTP/DNS client and TCP endpoint; does not forward transit traffic or host services.";
        case DeviceType::Printer: return "Printer: receives TCP traffic and exposes an HTTP status endpoint; no DNS server or client traffic.";
        case DeviceType::IoT: return "IoT endpoint: client traffic and HTTP telemetry/status; no DNS server or transit forwarding.";
        case DeviceType::NetworkSegment: return "Passive network segment: transit connectivity only; no application services or TCP endpoint.";
    }
    return "Invalid device type";
}

} // namespace kns
