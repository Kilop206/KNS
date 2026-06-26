#include <memory>
#include <cstddef>
#include <queue>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <utility>
#include <sstream>
#include <iomanip>

#include "engine/core/SimulationEngine.hpp"

#include "network/transport/tcp/TCPSession.hpp"
#include "engine/core/Event.hpp"
#include "engine/events/PacketReceivedEvent.hpp"
#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/events/TCPHandshakeEvent.hpp"
#include "network/Packet.hpp"
#include "enums/TCPState.hpp"
#include "network/transport/tcp/TCPSession.hpp"

namespace kns
{
    double SimulationEngine::random() {
        return (double)rand() / RAND_MAX;
    }

    double SimulationEngine::get_loss_prob() const {
        return loss_prob;
    }

    SimulationEngine::SimulationEngine(const Topology& topology)
    : topology_(topology) {
        int n = topology_.size();

        routing_tables_.resize(n);

        Routing routing;

        for (int u = 0; u < n; ++u) {
            routing_tables_[u] = routing.buildRoutingTable(topology_, u);
        }
    }

    void SimulationEngine::schedule(std::unique_ptr<Event> event) {
        event_queue_.schedule(std::move(event));
    }

    double SimulationEngine::now() const {
        return clock_.now();
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
            clock_.setTime(event->getTimestamp());
            event->execute(*this);
        }
    }

    bool SimulationEngine::processEvent()
    {
        if (event_queue_.hasEvents())
        {
            auto event = event_queue_.next();

            std::cout
                << "[PROCESS EVENT] t="
                << event->getTimestamp()
                << '\n';

            clock_.setTime(event->getTimestamp());

            event->execute(*this);

            return true;
        }

        return false;
    }

    double SimulationEngine::peekNextEventTime() const {
        return event_queue_.peekTimestamp();
    }

    double SimulationEngine::compute_arrival_time(const Packet& pkt, const Link& link, double now) {
        double transmission =
            (pkt.packet_size_bytes * 8.0) / (link.bandwidth_mbps * 1e6);

        double propagation = link.delay_ms / 1000.0;

        return (now + propagation + transmission);
    }

    void SimulationEngine::sendPacket(const Packet& pkt, const Link& link, double now) {
        stats_.packets_sent++;

        const int next_node = link.getOtherNode(pkt.current_node);

        double arrival_time = compute_arrival_time(pkt, link, now);

        emitPacketEvent(pkt, pkt.session_id, pkt.current_node, next_node, arrival_time);

        if (link.should_drop()) {
            stats_.packets_lost++;

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6)
                << "[DROPPED] Packet from " << pkt.source
                << " to " << pkt.destination
                << " at time " << now;
            std::cout << oss.str() << '\n';
            return;
        }

        Packet new_pkt = pkt;
        new_pkt.current_node = next_node;
        new_pkt.hop_count++;
        new_pkt.departure_time = now;

        auto event = std::make_unique<PacketReceivedEvent>(
            arrival_time,
            new_pkt
        );

        packets_in_transit.push_back(PacketTravelInfo{now,
                                                        arrival_time,
                                                        pkt.current_node,
                                                        next_node,
                                                        pkt.packet_type
                                                    });

        std::cout
            << "[SEND] type="
                << static_cast<int>(pkt.packet_type)
                << " "
                << pkt.current_node
                << " -> "
                << next_node
                << " arrival="
                << arrival_time
                << '\n';

        event_queue_.schedule(std::move(event));

        std::cout
            << "[SCHEDULED] type="
            << static_cast<int>(pkt.packet_type)
            << '\n';
    }

    void SimulationEngine::exportStatsCSV(const RunConfig& runConfig) {
        std::ofstream file(runConfig.filename, std::ios::out);

        file << "Packets Sent,Packets Delivered,Packets Lost,Delivery Rate,Loss Rate,Average Latency,Seed\n";

        double delivery_rate = stats_.packets_sent > 0
            ? (double)stats_.packets_delivered / stats_.packets_sent
            : 0.0;

        double loss_rate = stats_.packets_sent > 0
            ? (double)stats_.packets_lost / stats_.packets_sent
            : 0.0;

        double avg_latency = stats_.packets_delivered > 0
            ? stats_.total_latency / stats_.packets_delivered
            : 0.0;

        file << stats_.packets_sent << ","
            << stats_.packets_delivered << ","
            << stats_.packets_lost << ","
            << delivery_rate << ","
            << loss_rate << ","
            << avg_latency << ","
            << runConfig.seed  << "\n";
    }

    bool SimulationEngine::hasEvents() const {
        return event_queue_.hasEvents();
    }

    const std::vector<PacketTravelInfo>& SimulationEngine::getPacketsInTransit() const {
        return packets_in_transit;
    }

    void SimulationEngine::removePacketInTransit(double departure_time, double arrival_time) {
        for (std::size_t i = 0; i < packets_in_transit.size(); ++i) {
            if (packets_in_transit[i].departure_time == departure_time &&
                packets_in_transit[i].arrival_time == arrival_time) {
                packets_in_transit.erase(packets_in_transit.begin() + i);
                break;
                }
        }
    }

    void SimulationEngine::setGlobalLossProb(float value) {
        if (globalLossProb == value) return;

        globalLossProb = value;
        topology_.setGlobalLossProb(value);
    }

    void SimulationEngine::setGlobalPacketSize(int value) {
        globalPacketSize = value;
    }

    void SimulationEngine::setLatencyObserver(std::function<void(double)> observer) {
        latencyObserver_ = std::move(observer);
    }

    void SimulationEngine::notifyLatencyDelivered(double latency) {
        if (latencyObserver_) {
            latencyObserver_(latency);
        }
    }

    int SimulationEngine::getGlobalPacketSize() const {
        return globalPacketSize;
    }

    void SimulationEngine::startTCPConnection(
        int source,
        int dest
    )
    {
        auto& session = createTCPSession(source, dest);

        std::cout
            << "[TCP] Creating session "
            << session.getSession_id()
            << " "
            << source
            << " -> "
            << dest
            << '\n';

        schedule(
            std::make_unique<TCPHandshakeEvent>(
                now(),
                source,
                dest,
                session.getSession_id()
            )
        );
    }

    void SimulationEngine::setPacketObserver(
        std::function<void(const Packet&, uint64_t, int, int, double, double)> observer) {
        packetObserver = observer;
    }

    void SimulationEngine::emitPacketEvent(const Packet& p, int from, int to, double departure_time, double arrival_time)
    {
        if (packetObserver) {
            packetObserver(p, p.session_id, from, to, departure_time, arrival_time);
        }
    }

    void SimulationEngine::generatePackets(
        double startTime,
        TCPSession& session
    )
    {

        for (unsigned int i = 0; i < kPacketsPerRoute; ++i)
        {
            schedule(
                std::make_unique<PacketGenerationEvent>(
                    startTime + i * 0.02,
                    session.getSource(),
                    session.getDestination(),
                    session.getSession_id()
                )
            );
        }
    }

    TCPSession& SimulationEngine::createTCPSession(int source, int destination) {
        ++next_session_id;
        TCPSession session(next_session_id, source, destination, TCPState::CLOSED);

        sessions.insert({next_session_id, session});

        return getTCPSession(next_session_id);
    }

    TCPSession& SimulationEngine::getTCPSession(std::uint64_t session_id) {
        auto it = sessions.find(session_id);
        
        if (it == sessions.end()) {
            throw std::runtime_error("Session doesn't exist");
        }

        return it->second;
    }

    const std::map<int, TCPSession>& SimulationEngine::getTCPSessions() const {
        return sessions;
    }

    bool SimulationEngine::hasTCPSession(std::uint64_t session_id) const {
        auto it = sessions.find(session_id);

        return (it != sessions.end());
    }

    int SimulationEngine::getPacketsPerRoute() const {
        return kPacketsPerRoute;
    }

    void SimulationEngine::validateSimulation() const {
        std::size_t total_sessions = sessions.size();
        std::size_t established_sessions = 0;

        for (const auto& [id, session] : sessions)
        {
            (void)id;

            if (session.getState() == TCPState::ESTABLISHED)
            {
                ++established_sessions;
            }
        }

        const int expected_data_packets =
            established_sessions * static_cast<std::size_t>(kPacketsPerRoute);

        std::cout << "\n===== VALIDATION REPORT =====\n";
        std::cout << "Total sessions created: " << total_sessions << '\n';
        std::cout << "Sessions established:   " << established_sessions << '\n';
        std::cout << "Packets per session:    " << kPacketsPerRoute << '\n';
        std::cout << "Expected DATA packets:   " << expected_data_packets << '\n';
        std::cout << "Packets sent:            " << stats_.packets_sent << '\n';
        std::cout << "Packets delivered:       " << stats_.packets_delivered << '\n';
        std::cout << "Packets lost:            " << stats_.packets_lost << '\n';

        const bool sessions_ok = (total_sessions > 0) && (established_sessions == total_sessions);
        const bool traffic_ok =
            stats_.packets_delivered >= expected_data_packets &&
            stats_.packets_lost == 0;

        if (sessions_ok && traffic_ok)
        {
            std::cout << "Result: VALIDATION PASSED\n";
        }
        else
        {
            std::cout << "Result: VALIDATION FAILED\n";

            if (!sessions_ok)
            {
                std::cout << " - Not all sessions reached ESTABLISHED.\n";
            }

            if (stats_.packets_delivered < expected_data_packets)
            {
                std::cout << " - Delivered packets are below expected DATA packets.\n";
            }

            if (stats_.packets_lost != 0)
            {
                std::cout << " - There are lost packets.\n";
            }
        }

        std::cout << "============================\n";
    }

    void SimulationEngine::advanceTime(double time)
    {
        clock_.setTime(time);
    }
}