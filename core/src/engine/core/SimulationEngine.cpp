#include "engine/core/SimulationEngine.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/events/PacketReceivedEvent.hpp"
#include "engine/events/TCPHandshakeEvent.hpp"
#include "engine/events/TCPConnectionCloseEvent.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Random.hpp"
#include "network/Routing.hpp"
#include "network/Topology.hpp"
#include "network/transport/tcp/TCPSession.hpp"

namespace kns {

    double SimulationEngine::random() {
        return Random::uniform01();
    }

    double SimulationEngine::get_loss_prob() const {
        return loss_prob;
    }

    SimulationEngine::SimulationEngine(const Topology& topology)
        : topology_(topology)
    {
        const int n = topology_.size();
        routing_tables_.resize(n);

        Routing routing;
        for (int u = 0; u < n; ++u) {
            routing_tables_[u] = routing.buildRoutingTable(topology_, u);
        }
    }

    void SimulationEngine::schedule(std::unique_ptr<Event> event) {
        if (!event) {
            throw std::invalid_argument("Cannot schedule null event");
        }

        KNS_DEBUG_LOG(
            "[QUEUE PUSH] "
            << event->getName()
            << " t="
            << event->getTimestamp()
            << '\n');

        event_queue_.schedule(std::move(event));
    }

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

        file << "Packets Sent,Packets Delivered,Packets Lost,Delivery Rate,Loss Rate,Average Latency,Seed\n";

        const double delivery_rate = stats_.packets_sent > 0
            ? static_cast<double>(stats_.packets_delivered) / stats_.packets_sent
            : 0.0;

        const double loss_rate = stats_.packets_sent > 0
            ? static_cast<double>(stats_.packets_lost) / stats_.packets_sent
            : 0.0;

        const double avg_latency = stats_.packets_delivered > 0
            ? stats_.total_latency / stats_.packets_delivered
            : 0.0;

        file << stats_.packets_sent << ","
            << stats_.packets_delivered << ","
            << stats_.packets_lost << ","
            << delivery_rate << ","
            << loss_rate << ","
            << avg_latency << ","
            << runConfig.seed << "\n";
    }

    bool SimulationEngine::hasEvents() const {
        return event_queue_.hasEvents();
    }

    const std::vector<PacketTravelInfo>& SimulationEngine::getPacketsInTransit() const {
        return packets_in_transit;
    }

    bool SimulationEngine::removePacketInTransit(
        double departure_time,
        double arrival_time,
        int& from,
        int& to
    ) {
        for (std::size_t i = 0; i < packets_in_transit.size(); ++i) {
            if (packets_in_transit[i].departure_time == departure_time &&
                packets_in_transit[i].arrival_time == arrival_time) {
                from = packets_in_transit[i].link_from;
                to   = packets_in_transit[i].link_to;

                packets_in_transit.erase(
                    packets_in_transit.begin() + static_cast<std::ptrdiff_t>(i)
                );
                return true;
            }
        }

        return false;
    }

    void SimulationEngine::setGlobalLossProb(float value) {
        if (globalLossProb == value) {
            return;
        }

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

    void SimulationEngine::startTCPConnection(int source, int dest) {
        auto& session = createTCPSession(source, dest);

        constexpr double kHandshakeSpacing = 0.03;

        const double startTime = now() + handshake_offset_;
        handshake_offset_ += kHandshakeSpacing;

        schedule(std::make_unique<TCPHandshakeEvent>(
            startTime,
            source,
            dest,
            session.getSession_id()
        ));
    }

    void SimulationEngine::setPacketObserver(
        std::function<void(const Packet&, uint64_t, int, int, double, double)> observer
    ) {
        packetObserver = std::move(observer);
    }

    void SimulationEngine::emitPacketEvent(
        const Packet& p,
        int from,
        int to,
        double departure_time,
        double arrival_time
    ) {
        if (packetObserver) {
            packetObserver(p, p.session_id, from, to, departure_time, arrival_time);
        }
    }

    void SimulationEngine::generatePackets(
        double startTime,
        TCPSession& session
    )
    {
        if (session.hasGeneratedTraffic()) {
            return;
        }

        session.markTrafficGenerated();

        for (unsigned int i = 0; i < kPacketsPerRoute; ++i) {
            schedule(
                std::make_unique<PacketGenerationEvent>(
                    startTime + static_cast<double>(i) * 0.02,
                    session.getSource(),
                    session.getDestination(),
                    session.getSession_id()
                )
            );
        }

        const double close_time =
            startTime +
            static_cast<double>(kPacketsPerRoute) * 0.02 +
            0.05;

        schedule(
            std::make_unique<TCPConnectionCloseEvent>(
                close_time,
                session.getSession_id()
            )
        );
    }

    TCPSession& SimulationEngine::createTCPSession(int source, int destination) {
        ++next_session_id;

        TCPSession session(
            next_session_id,
            source,
            destination,
            TCPState::CLOSED
        );

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

    const std::map<std::uint64_t, TCPSession>& SimulationEngine::getTCPSessions() const {
        return sessions;
    }

    bool SimulationEngine::hasTCPSession(std::uint64_t session_id) const {
        return sessions.find(session_id) != sessions.end();
    }

    int SimulationEngine::getPacketsPerRoute() const {
        return kPacketsPerRoute;
    }

    ValidationReport SimulationEngine::validateSimulation() const {
        ValidationReport report;
        report.total_sessions = sessions.size();

        for (const auto& [id, session] : sessions) {
            (void)id;
            if (session.hasGeneratedTraffic()) {
                ++report.completed_sessions;
            }
        }

        report.expected_data_packets =
            static_cast<int>(report.completed_sessions) *
            static_cast<int>(kPacketsPerRoute);

        report.packets_sent = stats_.packets_sent;
        report.packets_delivered = stats_.packets_delivered;
        report.packets_lost = stats_.packets_lost;

        report.sessions_ok =
            (report.total_sessions > 0) &&
            (report.completed_sessions == report.total_sessions);

        report.traffic_ok =
            report.packets_delivered >= report.expected_data_packets &&
            report.packets_lost == 0;

        std::cout << "\n===== VALIDATION REPORT =====\n";
        std::cout << "Total sessions created: " << report.total_sessions << '\n';
        std::cout << "Sessions with traffic:  " << report.completed_sessions << '\n';
        std::cout << "Packets per session:    " << kPacketsPerRoute << '\n';
        std::cout << "Expected DATA packets:   " << report.expected_data_packets << '\n';
        std::cout << "Packets sent:            " << report.packets_sent << '\n';
        std::cout << "Packets delivered:       " << report.packets_delivered << '\n';
        std::cout << "Packets lost:            " << report.packets_lost << '\n';

        if (report.passed()) {
            std::cout << "Result: VALIDATION PASSED\n";
        } else {
            std::cout << "Result: VALIDATION FAILED\n";

            if (!report.sessions_ok) {
                std::cout << " - Not all sessions generated traffic.\n";
            }

            if (report.packets_delivered < report.expected_data_packets) {
                std::cout << " - Delivered packets are below expected DATA packets.\n";
            }

            if (report.packets_lost != 0) {
                std::cout << " - There are lost packets.\n";
            }
        }

        std::cout << "============================\n";

        return report;
    }

    void SimulationEngine::advanceTime(double time) {
        clock_.setTime(time);
    }
} // namespace kns
