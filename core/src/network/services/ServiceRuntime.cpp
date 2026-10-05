#include "network/services/ServiceRuntime.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/PacketReceivedEvent.hpp"
#include "network/utils/PacketUtils.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace kns {
namespace {
bool active(const SimulationEngine& engine, int id) {
    const auto* node = engine.getTopology().getNode(id);
    return node && node->isActive();
}

const NetworkService* findService(const SimulationEngine& engine, int node, ServiceKind kind, int port) {
    if (!active(engine, node)) return nullptr;
    for (const auto& service : engine.getTopology().getNode(node)->getServices())
        if (service.kind == kind && service.port == port && service.enabled) return &service;
    return nullptr;
}

void send(SimulationEngine& engine, Packet packet) {
    if (!active(engine, packet.source) || !active(engine, packet.destination)) return;
    packet.creation_time = engine.now();
    if (packet.source == packet.destination) {
        engine.getStats().packets_sent++;
        engine.schedule(std::make_unique<PacketReceivedEvent>(engine.now(), std::move(packet)));
    } else {
        PacketUtils::sendPacketThroughTopology(engine, packet);
    }
}

class ServiceTimeoutEvent final : public Event {
    std::uint64_t id_;
public:
    ServiceTimeoutEvent(double time, std::uint64_t id) : Event(time), id_(id) {}
    void execute(SimulationEngine& engine) override { engine.networkServices().timeout(id_, engine.now()); }
    const char* getName() const noexcept override { return "ServiceTimeout"; }
};

class ServiceReplyEvent final : public Event {
    Packet packet_;
    std::uint64_t revision_;
public:
    ServiceReplyEvent(double time, Packet packet, std::uint64_t revision)
        : Event(time), packet_(std::move(packet)), revision_(revision) {}
    void execute(SimulationEngine& engine) override {
        const auto* current = findService(engine, packet_.source, packet_.service->kind, packet_.service->port);
        // Stopping, removing or editing a service invalidates its queued replies.
        if (current && engine.getTopology().getNode(packet_.source)->getServicesRevision() == revision_)
            send(engine, std::move(packet_));
    }
    const char* getName() const noexcept override { return "ServiceReply"; }
};
}

const char* serviceRequestStateName(ServiceRequestState state) {
    switch (state) {
        case ServiceRequestState::Pending: return "pending";
        case ServiceRequestState::Complete: return "complete";
        case ServiceRequestState::TimedOut: return "timeout";
    }
    return "unknown";
}

std::uint64_t ServiceRuntime::request(SimulationEngine& engine, int source, int destination,
    ServiceKind kind, int port, std::string payload, double timeout_seconds) {
    (void)serviceKindName(kind);
    if (!active(engine, source) || !active(engine, destination)) throw std::invalid_argument("Device is missing or inactive");
    if (port < 1 || port > 65535) throw std::invalid_argument("Port must be in [1, 65535]");
    if (payload.empty() || payload.size() > 512) throw std::invalid_argument("Request must contain 1-512 bytes");
    if (kind == ServiceKind::Dns) payload = normalizeDnsName(std::move(payload));
    else if (payload[0] != '/' || payload.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("HTTP GET requires a path starting with / without whitespace");
    if (!std::isfinite(timeout_seconds) || timeout_seconds <= 0 || timeout_seconds > 3600 ||
        !std::isfinite(engine.now() + timeout_seconds)) throw std::invalid_argument("Timeout must be in (0, 3600] seconds");
    if (requests_.size() >= 256) {
        const auto old = std::find_if(requests_.begin(), requests_.end(), [](const auto& entry) {
            return entry.second.state != ServiceRequestState::Pending;
        });
        if (old == requests_.end()) throw std::invalid_argument("Maximum 256 pending service requests");
        requests_.erase(old);
    }
    const auto id = next_id_++;
    requests_.emplace(id, ServiceRequest{id, source, destination, kind, port, engine.now(), 0,
        ServiceRequestState::Pending, 0, {}});
    engine.schedule(std::make_unique<ServiceTimeoutEvent>(engine.now() + timeout_seconds, id));
    Packet packet(source, destination, source, engine.now(), 28 + static_cast<int>(payload.size()), 0);
    packet.service = ServiceMessage{id, kind, port, false, 0, std::move(payload)};
    send(engine, std::move(packet));
    return id;
}

void ServiceRuntime::timeout(std::uint64_t id, double now) {
    const auto found = requests_.find(id);
    if (found != requests_.end() && found->second.state == ServiceRequestState::Pending) {
        found->second.state = ServiceRequestState::TimedOut;
        found->second.finished_at = now;
        found->second.response = "No response before the simulated timeout";
    }
}

void ServiceRuntime::receive(SimulationEngine& engine, const Packet& packet) {
    if (!packet.service || !active(engine, packet.destination)) return;
    const auto& message = *packet.service;
    const auto found = requests_.find(message.request_id);
    if (found == requests_.end() || found->second.state != ServiceRequestState::Pending) return;
    auto& request = found->second;
    if (request.kind != message.kind || request.port != message.port) return;
    if (message.response) {
        if (packet.source != request.destination || packet.destination != request.source) return;
        request.state = ServiceRequestState::Complete;
        request.finished_at = engine.now();
        request.status = message.status;
        request.response = message.payload;
        return;
    }
    if (packet.source != request.source || packet.destination != request.destination) return;
    const auto* service = findService(engine, packet.destination, message.kind, message.port);
    if (!service) return;
    ServiceMessage reply{message.request_id, message.kind, message.port, true, 0, {}};
    if (message.kind == ServiceKind::Http) {
        const auto page = service->pages.find(message.payload);
        reply.status = page == service->pages.end() ? 404 : page->second.status;
        reply.payload = page == service->pages.end() ? "Not Found" : page->second.body;
    } else {
        const auto record = service->records.find(message.payload);
        reply.status = record == service->records.end() ? 3 : 0; // NXDOMAIN / NOERROR
        reply.payload = record == service->records.end() ? "NXDOMAIN" : record->second;
    }
    Packet response(packet.destination, packet.source, packet.destination, engine.now(),
        28 + static_cast<int>(reply.payload.size()), 0);
    response.service = std::move(reply);
    engine.schedule(std::make_unique<ServiceReplyEvent>(engine.now() + service->delay_ms / 1000.0,
        std::move(response), engine.getTopology().getNode(packet.destination)->getServicesRevision()));
}
} // namespace kns
