#pragma once

#include <queue>
#include <memory>
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>
#include <map>

#include "network/transport/tcp/TCPSession.hpp"
#include "network/Topology.hpp"
#include "network/Routing.hpp"
#include "engine/core/Event.hpp"
#include "engine/core/Stats.hpp"
#include "engine/core/EventQueue.hpp"
#include "engine/core/SimulationClock.hpp"
#include "network/Packet.hpp"
#include "network/Link.hpp"
#include "engine/core/RunConfig.hpp"
#include "network/PacketTravelInfo.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPConnectionKey.hpp"

struct PacketSpec;

namespace kns {

    struct ValidationReport {
        std::size_t total_sessions = 0;
        std::size_t completed_sessions = 0;
        int expected_data_packets = 0;
        int packets_sent = 0;
        int packets_delivered = 0;
        int packets_lost = 0;
        bool sessions_ok = false;
        bool traffic_ok = false;

        bool passed() const noexcept {
            return sessions_ok && traffic_ok;
        }
    };

    class SimulationEngine {
    private:

        double loss_prob = 0.01;

        // Current simulation time.
        SimulationClock clock_;

        // Network topology.
        Topology topology_;

        // Routing tables for each node.
        std::vector<std::vector<Routing::RoutingEntry>> routing_tables_;

        // Statistics for the simulation
        Stats stats_;

        // Event queue for managing scheduled events
        EventQueue event_queue_;

        //Vector with info about the packets
        std::vector<PacketTravelInfo> packets_in_transit;

        float globalLossProb = 0.0f;

        int globalPacketSize = 0;

        std::function<void(double)> latencyObserver_;

        std::function<void(const Packet&, std::uint64_t, int, int, double, double)> packetObserver;

        std::map<std::uint64_t, TCPSession> sessions;

        uint64_t next_session_id = 0;

        double handshake_offset_ = 0.0;

        unsigned int kPacketsPerRoute = 20;

        /// Metric used by Dijkstra when (re)building routing tables.
        RoutingMetric routing_metric_ = RoutingMetric::Delay;

    public:
        double random();

        double get_loss_prob() const;

        explicit SimulationEngine(const Topology& topology);

        void schedule(std::unique_ptr<Event> event);

        void run();

        bool processEvent();

        double peekNextEventTime() const;

        double now() const;

        int getNextHop(int current, int destination) const;

        Topology& getTopology();

        const Topology& getTopology() const;

        Stats& getStats();

        double compute_arrival_time(const Packet& pkt, const Link& link, double now);

        bool sendPacket(const Packet& pkt, Link& link, double now);

        void exportStatsCSV(const RunConfig& runConfig);

        bool hasEvents() const;

        const std::vector<PacketTravelInfo>& getPacketsInTransit() const;

        bool removePacketInTransit(double departure_time,
                                    double arrival_time,
                                    int& from,
                                    int& to,
                                    std::uint64_t& link_id);

        void setGlobalLossProb(float value);

        void setGlobalPacketSize(int value);

        void setLatencyObserver(std::function<void(double)> observer);

        void notifyLatencyDelivered(double latency);

        int getGlobalPacketSize() const;

        void startTCPConnection(int source, int dest);

        void setPacketObserver(
            std::function<void(const Packet&, uint64_t session_id, int from, int to, double departure_time, double arrival_time)> observer
        );

        void emitPacketEvent(const Packet& p, int from, int to, double departure_time, double arrival_time);

        void generatePackets(double startTime, TCPSession& session);

        TCPSession& createTCPSession(int source, int destination);

        TCPSession& getTCPSession(std::uint64_t session_id);

        const std::map<std::uint64_t, TCPSession>& getTCPSessions() const;

        bool hasTCPSession(std::uint64_t session_id) const;

        int getPacketsPerRoute() const;

        ValidationReport validateSimulation() const;

        void advanceTime(double time);

        // GUI / topology modification helpers
        int createNode();
        bool deleteNode(int id);
        Topology::LinkPtr createLink(int a, int b, double bandwidth_mbps, double delay_ms, double loss_prob = 0.0, LinkMode mode = LinkMode::FULL_DUPLEX);
        bool deleteLink(int a, int b);
        bool toggleLinkUp(int a, int b, bool up);
        void rebuildRoutingTables();

        /// Change the routing metric and immediately rebuild all routing tables.
        void setRoutingMetric(RoutingMetric metric);
        RoutingMetric getRoutingMetric() const noexcept;

        /// Schedule a link failure (up=false) or recovery (up=true) at the
        /// given simulation time. This is the event-driven equivalent of
        /// toggleLinkUp() for use inside a running simulation.
        void scheduleLinkFailure(double at_time, int node_a, int node_b, bool up);
    };
}