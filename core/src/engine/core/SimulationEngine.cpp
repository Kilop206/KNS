#include "engine/core/SimulationEngine.hpp"

#include <fstream>
#include <iostream>
#include <algorithm>

#include "engine/events/PacketReceivedEvent.hpp"
#include "engine/core/RunConfig.hpp"
#include "engine/events/TCPHandshakeEvent.hpp"
#include "engine/events/TCPHandshakeTimeoutEvent.hpp"
#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/events/TCPConnectionCloseEvent.hpp"
#include "engine/events/LinkFailureEvent.hpp"
#include "network/utils/PacketUtils.hpp"
#include "engine/core/Random.hpp"

namespace kns {

    SimulationEngine::SimulationEngine(const Topology& topology)
        : loss_prob(0.01),
        clock_(),
        topology_(topology),
        routing_tables_(),
        stats_(),
        event_queue_(),
        packets_in_transit(),
        globalLossProb(0.0f),
        globalPacketSize(0),
        latencyObserver_(nullptr),
        packetObserver(nullptr),
        sessions(),
        next_session_id(0),
        handshake_offset_(0.0),
        kPacketsPerRoute(20)
    {
        rebuildRoutingTables();
    }

    double SimulationEngine::now() const {
        return clock_.now();
    }

    double SimulationEngine::random() {
        return Random::uniform01();
    }

    double SimulationEngine::get_loss_prob() const {
        return loss_prob;
    }

    Topology& SimulationEngine::getTopology() {
        return topology_;
    }

    const Topology& SimulationEngine::getTopology() const {
        return topology_;
    }

    Stats& SimulationEngine::getStats() {
        return stats_;
    }

    bool SimulationEngine::hasEvents() const {
        return event_queue_.hasEvents();
    }

    int SimulationEngine::getNextHop(int current, int destination) const {
        if (current < 0 || static_cast<std::size_t>(current) >= routing_tables_.size()) {
            return -1;
        }
        const auto& rt_row = routing_tables_[static_cast<std::size_t>(current)];
        if (destination < 0 || static_cast<std::size_t>(destination) >= rt_row.size()) {
            return -1;
        }
        return rt_row[static_cast<std::size_t>(destination)].next_hop;
    }

    const std::vector<PacketTravelInfo>& SimulationEngine::getPacketsInTransit() const {
        return packets_in_transit;
    }

    bool SimulationEngine::removePacketInTransit(double departure_time,
                                    double arrival_time,
                                    int& from,
                                    int& to,
                                    std::uint64_t& link_id) {
        for (auto it = packets_in_transit.begin(); it != packets_in_transit.end(); ++it) {
            if (it->departure_time == departure_time && it->arrival_time == arrival_time) {
                from = it->from_node;
                to = it->to_node;
                link_id = it->link_id;
                packets_in_transit.erase(it);
                return true;
            }
        }
        return false;
    }

    void SimulationEngine::run() {
        while (event_queue_.hasEvents()) {
            auto event = event_queue_.next();
            if (!event) {
                break;
            }

            clock_.setTime(event->getTimestamp());
            event->execute(*this);
        }
    }

    bool SimulationEngine::processEvent() {
        if (!event_queue_.hasEvents()) {
            return false;
        }

        auto event = event_queue_.next();
        if (!event) {
            return false;
        }

        // Debug logging macro removed to avoid build issues in environments without the macro
        clock_.setTime(event->getTimestamp());
        event->execute(*this);
        return true;
    }

    double SimulationEngine::peekNextEventTime() const {
        return event_queue_.peekTimestamp();
    }

    double SimulationEngine::compute_arrival_time(
        const Packet& pkt,
        const Link& link,
        double now
    ) {
        const double transmission =
            (static_cast<double>(pkt.packet_size_bytes) * 8.0) /
            (link.getBandwidthMbps() * 1e6);

        const double propagation = link.getDelayMs() / 1000.0;

        return now + propagation + transmission;
    }

    bool SimulationEngine::sendPacket(const Packet& pkt, Link& link, double now)
    {
        if (!link.isUp()) {
            return false;
        }

        const int next_node = link.getOtherNode(pkt.current_node);
        if (next_node == -1) {
            return false;
        }

        const double transmission_time =
            (static_cast<double>(pkt.packet_size_bytes) * 8.0) /
            (link.getBandwidthMbps() * 1e6);

        const double propagation_time = link.getDelayMs() / 1000.0;

        const double actual_departure_time =
            link.getNextAvailableTime(pkt.current_node, next_node, now);

        const double arrival_time =
            actual_departure_time + transmission_time + propagation_time;

        link.reserveTransmission(
            pkt.current_node,
            next_node,
            arrival_time
        );

        Packet new_pkt = pkt;
        new_pkt.current_node = next_node;
        new_pkt.previous_node = pkt.current_node;
        new_pkt.link_id = link.getId();
        new_pkt.hop_count++;
        new_pkt.departure_time = actual_departure_time;
        new_pkt.arrival_time = arrival_time;

        if (pkt.hop_count == 0) {
            stats_.packets_sent++;
        }

        if (link.should_drop()) {
            stats_.packets_lost++;
            return false;
        }

        // Record in the FIFO queue so the arrival event can release the exact slot.
        link.enqueueTransmission(
            pkt.current_node, next_node,
            actual_departure_time, arrival_time
        );

        packets_in_transit.push_back(PacketTravelInfo{
            actual_departure_time,
            arrival_time,
            pkt.current_node,
            next_node,
            pkt.packet_type,
            link.getId()
        });

        emitPacketEvent(
            pkt,
            pkt.current_node,
            next_node,
            actual_departure_time,
            arrival_time
        );

        event_queue_.schedule(
            std::make_unique<PacketReceivedEvent>(arrival_time, new_pkt)
        );

        return true;
    }

    void SimulationEngine::exportStatsCSV(const RunConfig& runConfig) {
        std::ofstream file(runConfig.filename, std::ios::out);
        if (!file.is_open()) {
            std::cerr << "SimulationEngine::exportStatsCSV: failed to open file " << runConfig.filename << std::endl;
            return;
        }

        // Header
        file << "packets_sent,packets_delivered,packets_lost,total_latency,avg_latency,packets_in_transit,total_sessions\n";

        // Values
        const int sent = stats_.packets_sent;
        const int delivered = stats_.packets_delivered;
        const int lost = stats_.packets_lost;
        const double total_latency = stats_.total_latency;
        const double avg_latency = (delivered > 0) ? (total_latency / static_cast<double>(delivered)) : 0.0;
        const std::size_t in_transit = packets_in_transit.size();
        const std::size_t total_sessions = sessions.size();

        file << sent << ','
             << delivered << ','
             << lost << ','
             << total_latency << ','
             << avg_latency << ','
             << in_transit << ','
             << total_sessions << '\n';

        file.close();
    }

    void SimulationEngine::advanceTime(double time) {
        clock_.setTime(time);
    }

    void SimulationEngine::schedule(std::unique_ptr<Event> event) {
        event_queue_.schedule(std::move(event));
    }

    void SimulationEngine::setGlobalLossProb(float value) {
        globalLossProb = value;
        topology_.setGlobalLossProb(static_cast<double>(value));
    }

    void SimulationEngine::setGlobalPacketSize(int value) {
        globalPacketSize = value;
    }

    void SimulationEngine::setLatencyObserver(std::function<void(double)> observer) {
        latencyObserver_ = std::move(observer);
    }

    void SimulationEngine::notifyLatencyDelivered(double latency) {
        if (latencyObserver_) latencyObserver_(latency);
    }

    int SimulationEngine::getGlobalPacketSize() const {
        return globalPacketSize;
    }

    void SimulationEngine::setPacketObserver(
        std::function<void(const Packet&, uint64_t session_id, int from, int to, double departure_time, double arrival_time)> observer
    ) {
        packetObserver = std::move(observer);
    }

    void SimulationEngine::emitPacketEvent(const Packet& p, int from, int to, double departure_time, double arrival_time) {
        if (packetObserver) {
            // if a real session id is known in context, pass it instead of 0
            packetObserver(p, /*session_id*/ 0, from, to, departure_time, arrival_time);
        }
    }

    TCPSession& SimulationEngine::createTCPSession(int source, int destination) {
        uint64_t id = next_session_id++;
        sessions.emplace(id, TCPSession());
        // optional: initialize session endpoints if TCPSession exposes such methods
        return sessions.at(id);
    }

    TCPSession& SimulationEngine::getTCPSession(std::uint64_t session_id) {
        return sessions.at(session_id);
    }

    const std::map<std::uint64_t, TCPSession>& SimulationEngine::getTCPSessions() const {
        return sessions;
    }

    bool SimulationEngine::hasTCPSession(std::uint64_t session_id) const {
        return sessions.find(session_id) != sessions.end();
    }

    int SimulationEngine::getPacketsPerRoute() const {
        return static_cast<int>(kPacketsPerRoute);
    }

    ValidationReport SimulationEngine::validateSimulation() const {
        ValidationReport r;
        r.total_sessions = sessions.size();
        r.packets_sent = stats_.packets_sent;
        r.packets_delivered = stats_.packets_delivered;
        r.packets_lost = stats_.packets_lost;
        r.completed_sessions = 0;
        for (const auto& pair : sessions) {
            const auto state = pair.second.getState();
            if (state == TCPState::ESTABLISHED || state == TCPState::CLOSED) {
                r.completed_sessions++;
            }
        }
        r.sessions_ok = (r.total_sessions == 0) || (r.completed_sessions > 0);
        r.traffic_ok = (r.packets_delivered > 0);
        return r;
    }

    void SimulationEngine::startTCPConnection(int source, int dest) {
        TCPSession& session = createTCPSession(source, dest);
        schedule(std::make_unique<TCPHandshakeEvent>(now() + handshake_offset_, source, dest, session.getSession_id()));
        handshake_offset_ += 0.05;
    }

    TCPListener& SimulationEngine::startTCPListen(int node_id, int backlog) {
        auto [it, _] = listeners_.emplace(node_id, TCPListener(node_id, backlog));
        return it->second;
    }

    bool SimulationEngine::hasListener(int node_id) const noexcept {
        const auto it = listeners_.find(node_id);
        return it != listeners_.end() && it->second.isListening();
    }

    std::uint64_t SimulationEngine::acceptOnListener(int listening_node, int connecting_node) {
        auto it = listeners_.find(listening_node);
        if (it == listeners_.end()) return 0;
        return it->second.accept(connecting_node, *this);
    }

    void SimulationEngine::generatePackets(double /*startTime*/, TCPSession& /*session*/) {
        // No-op for now; startTCPConnection currently synthesizes packet events.
    }

    int SimulationEngine::createNode() {
        const int id = topology_.addNode();
        rebuildRoutingTables();
        return id;
    }

    bool SimulationEngine::deleteNode(int id) {
        const bool ok = topology_.removeNode(id);
        if (ok) rebuildRoutingTables();
        return ok;
    }

    Topology::LinkPtr SimulationEngine::createLink(int a, int b, double bandwidth_mbps, double delay_ms, double loss_prob, LinkMode mode) {
        auto ptr = topology_.addLinkPtr(a, b, bandwidth_mbps, delay_ms, loss_prob, mode);
        rebuildRoutingTables();
        return ptr;
    }

    bool SimulationEngine::deleteLink(int a, int b) {
        const bool ok = topology_.removeLink(a, b);
        if (ok) rebuildRoutingTables();
        return ok;
    }

    bool SimulationEngine::toggleLinkUp(int a, int b, bool up) {
        const bool ok = topology_.setLinkUp(a, b, up);
        if (ok) rebuildRoutingTables();
        return ok;
    }

    void SimulationEngine::rebuildRoutingTables() {
        const int n = topology_.size();
        routing_tables_.clear();
        routing_tables_.resize(static_cast<std::size_t>(n));
        Routing routing;
        for (int u = 0; u < n; ++u) {
            routing_tables_[static_cast<std::size_t>(u)] =
                routing.buildRoutingTable(topology_, u, routing_metric_);
        }
    }

    void SimulationEngine::setRoutingMetric(RoutingMetric metric) {
        routing_metric_ = metric;
        rebuildRoutingTables();
    }

    RoutingMetric SimulationEngine::getRoutingMetric() const noexcept {
        return routing_metric_;
    }

    void SimulationEngine::scheduleLinkFailure(double at_time, int node_a, int node_b, bool up) {
        schedule(std::make_unique<LinkFailureEvent>(at_time, node_a, node_b, up));
    }

} // namespace kns