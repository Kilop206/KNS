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

    class SimulationEngine {
    private:

        double loss_prob = 0.01;

        std::unordered_map<int, std::queue<Packet>> buffers;

        size_t max_queue_size = 50;

        // Current simulation time.
        SimulationClock clock_;

        // Network topology.
        Topology topology_;

        // Routing tables for each node.
        std::vector<std::vector<Routing::RoutingEntry>> routing_tables_;

        // Event comparison functor for priority queue.
        struct EventCompare {
            bool operator()(const std::unique_ptr<Event>& a,
                            const std::unique_ptr<Event>& b) const {
                if (a->getTimestamp() == b->getTimestamp()) {
                    return a->getId() > b->getId();
                }
                return a->getTimestamp() > b->getTimestamp();
            }
        };

        // Statistics for the simulation
        Stats stats_;

        // Event queue for managing scheduled events
        EventQueue event_queue_;

        //Vector with info about the packets
        std::vector<PacketTravelInfo> packets_in_transit;

        float globalLossProb = 0.0f;

        int globalPacketSize = 0;

        double simulation_speed_multiplier_ = 1.0;

        std::function<void(double)> latencyObserver_;

        std::unordered_map<TCPConnectionKey, std::uint64_t> tcp_connections_;

        std::function<void(const Packet&, std::uint64_t, int, int, double, double)> packetObserver;

        std::map<int, TCPSession> sessions;

        std::map<int, TCPSession> active_tcp_sessions;

        uint64_t next_session_id = 0;

        unsigned int kPacketsPerRoute = 20;
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

        void sendPacket(const Packet& pkt, Link& link, double now);

        void exportStatsCSV(const RunConfig& runConfig);

        bool hasEvents() const;

        const std::vector<PacketTravelInfo>& getPacketsInTransit() const;

        bool removePacketInTransit(double departure_time,
                                    double arrival_time,
                                    int& from,
                                    int& to);

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

        const std::map<int, TCPSession>& getTCPSessions() const;

        bool hasTCPSession(std::uint64_t session_id) const;

        int getPacketsPerRoute() const;

        void validateSimulation() const;

        void advanceTime(double time);
    };
}