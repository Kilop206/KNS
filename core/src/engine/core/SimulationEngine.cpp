#include "engine/core/SimulationEngine.hpp"

#include <fstream>

#include "engine/events/PacketReceivedEvent.hpp"
#include "engine/core/RunConfig.hpp"
#include "engine/events/TCPHandshakeEvent.hpp"
#include "engine/events/TCPHandshakeTimeoutEvent.hpp"
#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/events/TCPConnectionCloseEvent.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns {

    double SimulationEngine::now() const {
        return clock_.now();
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

    int SimulationEngine::getNextHop(int current, int destination) const {
        return routing_tables_[current][destination].next_hop;
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

        KNS_DEBUG_LOG(
            "[PROCESS EVENT] t="
            << event->getTimestamp()
            << '\n');

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

    void SimulationEngine::sendPacket(const Packet& pkt, Link& link, double now)
    {
        const int next_node = link.getOtherNode(pkt.current_node);
        if (next_node == -1) {
            return;
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
        new_pkt.hop_count++;
        new_pkt.departure_time = actual_departure_time;

        if (pkt.hop_count == 0) {
            stats_.packets_sent++;
        }

        if (link.should_drop()) {
            stats_.packets_lost++;
            return;
        }

        packets_in_transit.push_back(PacketTravelInfo{
            actual_departure_time,
            arrival_time,
            pkt.current_node,
            next_node,
            pkt.packet_type
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
    }

    void SimulationEngine::exportStatsCSV(const RunConfig& runConfig) {
        std::ofstream file(runConfig.filename, std::ios::out);
        if (!file.is_open()) {
            std::cerr << "SimulationEngine::exportStatsCSV: failed to open file " << runConfig.filename << std::endl;
            return;
        }
        
        file << "packets_sent,packets_delivered,packets_lost,total_latency,avg_latency,packets_in_transit,total_sessions\n";

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
        routing_tables_.resize(n);
        Routing routing;
        for (int u = 0; u < n; ++u) {
            routing_tables_[u] = routing.buildRoutingTable(topology_, u);
        }
    }

}