#pragma once

#include "network/services/NetworkService.hpp"
#include <cstdint>
#include <map>

namespace kns {
class SimulationEngine;
struct Packet;

// Internal simulation envelope, not a wire-compatible HTTP/DNS implementation.
struct ServiceMessage {
    std::uint64_t request_id = 0;
    ServiceKind kind = ServiceKind::Http;
    int port = 80;
    bool response = false;
    int status = 0;
    std::string payload;
};

enum class ServiceRequestState { Pending, Complete, TimedOut };
const char* serviceRequestStateName(ServiceRequestState state);

struct ServiceRequest {
    std::uint64_t id;
    int source;
    int destination;
    ServiceKind kind;
    int port;
    double started_at;
    double finished_at = 0;
    ServiceRequestState state = ServiceRequestState::Pending;
    int status = 0;
    std::string response;
};

class ServiceRuntime {
public:
    std::uint64_t request(SimulationEngine& engine, int source, int destination,
        ServiceKind kind, int port, std::string payload, double timeout_seconds = 5.0);
    void receive(SimulationEngine& engine, const Packet& packet);
    void timeout(std::uint64_t id, double now);
    const std::map<std::uint64_t, ServiceRequest>& requests() const { return requests_; }
private:
    std::uint64_t next_id_ = 1;
    std::map<std::uint64_t, ServiceRequest> requests_;
};
} // namespace kns
