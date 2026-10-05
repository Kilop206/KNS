#include "network/services/NetworkService.hpp"

#include <charconv>
#include <cmath>
#include <set>
#include <stdexcept>

namespace kns {
bool canHostService(DeviceType type, ServiceKind kind) noexcept {
    const auto capabilities = deviceCapabilities(type);
    return (kind == ServiceKind::Http && capabilities.http_server) ||
           (kind == ServiceKind::Dns && capabilities.dns_server);
}

void validateDeviceServices(DeviceType type, const std::vector<NetworkService>& services) {
    validateServices(services);
    for (const auto& service : services) {
        if (!canHostService(type, service.kind))
            throw std::invalid_argument(std::string(toString(type)) + " cannot host " + serviceKindName(service.kind) +
                " services; remove incompatible services before changing the device type");
    }
}

const char* serviceKindName(ServiceKind kind) {
    switch (kind) {
        case ServiceKind::Http: return "http";
        case ServiceKind::Dns: return "dns";
    }
    throw std::invalid_argument("Unknown service kind");
}

ServiceKind parseServiceKind(const std::string& name) {
    if (name == "http") return ServiceKind::Http;
    if (name == "dns") return ServiceKind::Dns;
    throw std::invalid_argument("Service kind must be http or dns");
}

std::string normalizeDnsName(std::string name) {
    if (!name.empty() && name.back() == '.') name.pop_back();
    if (name.empty() || name.size() > 253) throw std::invalid_argument("Invalid DNS name");
    std::size_t label = 0;
    char previous = '.';
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c == '.') {
            if (label == 0 || previous == '-') throw std::invalid_argument("Invalid DNS label");
            label = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-' && label > 0)) ||
                ++label > 63) throw std::invalid_argument("Invalid DNS label");
        }
        previous = c;
    }
    if (label == 0 || previous == '-') throw std::invalid_argument("Invalid DNS label");
    return name;
}

namespace {
bool validIPv4(const std::string& value) {
    std::size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        const auto end = value.find('.', start);
        if ((i == 3) != (end == std::string::npos)) return false;
        const auto length = (end == std::string::npos ? value.size() : end) - start;
        if (length == 0 || length > 3 || value[start] < '0' || value[start] > '9' ||
            (length > 1 && value[start] == '0')) return false;
        int part = 0;
        const auto result = std::from_chars(value.data() + start, value.data() + start + length, part);
        if (result.ec != std::errc{} || result.ptr != value.data() + start + length || part > 255) return false;
        start += length + 1;
    }
    return true;
}
}

void validateServices(const std::vector<NetworkService>& services) {
    if (services.size() > 32) throw std::invalid_argument("Maximum 32 services per device");
    std::set<std::string> names;
    std::set<std::pair<ServiceKind, int>> ports;
    for (const auto& service : services) {
        (void)serviceKindName(service.kind);
        if (service.name.empty() || service.name.size() > 64 ||
            service.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
            throw std::invalid_argument("Service name: use 1-64 letters, digits, underscores or hyphens");
        if (!names.insert(service.name).second) throw std::invalid_argument("Duplicate service name");
        if (service.port < 1 || service.port > 65535) throw std::invalid_argument("Port must be in [1, 65535]");
        if (!ports.emplace(service.kind, service.port).second) throw std::invalid_argument("Service port already configured for this protocol");
        if (!std::isfinite(service.delay_ms) || service.delay_ms < 0 || service.delay_ms > 60000)
            throw std::invalid_argument("Processing delay must be in [0, 60000] ms");
        if (service.pages.size() > 128 || service.records.size() > 128)
            throw std::invalid_argument("Maximum 128 pages or DNS records per service");
        if ((service.kind == ServiceKind::Http && !service.records.empty()) ||
            (service.kind == ServiceKind::Dns && !service.pages.empty()))
            throw std::invalid_argument("Entries do not match the service protocol");
        for (const auto& [path, page] : service.pages) {
            if (path.empty() || path[0] != '/' || path.size() > 512 || path.find_first_of(" \t\r\n") != std::string::npos)
                throw std::invalid_argument("HTTP path must start with / and contain no whitespace (maximum 512 bytes)");
            if (page.status < 200 || page.status > 599 || page.body.size() > 4096)
                throw std::invalid_argument("HTTP status must be in [200, 599]; body maximum is 4096 bytes");
        }
        for (const auto& [name, address] : service.records) {
            if (normalizeDnsName(name) != name) throw std::invalid_argument("DNS record names must be lowercase without a trailing dot");
            if (!validIPv4(address)) throw std::invalid_argument("DNS A record requires a valid IPv4 address");
        }
    }
}
} // namespace kns
