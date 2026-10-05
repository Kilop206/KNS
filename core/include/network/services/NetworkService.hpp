#pragma once

#include <map>
#include <string>
#include <vector>
#include "network/DeviceType.hpp"

namespace kns {
enum class ServiceKind { Http, Dns };
const char* serviceKindName(ServiceKind kind);
ServiceKind parseServiceKind(const std::string& name);

struct HttpPage {
    int status = 200;
    std::string body;
    bool operator==(const HttpPage&) const = default;
};

// Application configuration only; no sockets, executable scripts or host I/O.
struct NetworkService {
    std::string name;
    ServiceKind kind = ServiceKind::Http;
    int port = 80;
    bool enabled = true;
    double delay_ms = 0.0;
    std::map<std::string, HttpPage> pages;
    std::map<std::string, std::string> records;
    bool operator==(const NetworkService&) const = default;
};

std::string normalizeDnsName(std::string name);
void validateServices(const std::vector<NetworkService>& services);
bool canHostService(DeviceType type, ServiceKind kind) noexcept;
void validateDeviceServices(DeviceType type, const std::vector<NetworkService>& services);
} // namespace kns
