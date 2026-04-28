#pragma once

#include <queue>
#include <memory>
#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>

#include "network/Topology.hpp"
#include "network/Routing.hpp"
#include "engine/events/Event.hpp"
#include "engine/core/Stats.hpp"
#include "engine/core/EventQueue.hpp"
#include "engine/time/SimulationClock.hpp"
#include "network/Packet.hpp"
#include "network/Link.hpp"
#include "engine/core/RunConfig.hpp"
#include "network/PacketTravelInfo.hpp"
#include "network/tcp/TCPConnection.hpp"

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

        std::unordered_map<
            kns::TCPConnection,
            std::pair<int, int>,
            kns::TCPConnectionHash,
            kns::TCPConnectionEqual
        > tcp_connections_;

        std::function<void(const Packet&, int, int, double)> packetObserver;

    public:
        double random();

        double get_loss_prob() const;

        // Constructor that initializes the simulation engine with a given topology.
        explicit SimulationEngine(const Topology& topology);

        // Schedules a new event to be processed by the simulation engine.
        void schedule(std::unique_ptr<Event> event);

        // Runs the simulation by processing events from the event queue.
        void run();

       // Runs a single event
        void processEvent();

        // Returns the timestamp of the next scheduled event, if any.
        double peekNextEventTime() const;

        // Returns the current simulation time.
        double now() const;

        // Returns the next hop for a packet based on the routing table.
        int getNextHop(int current, int destination) const;

        // Returns a const reference to the topology.
        const Topology& getTopology() const;

        // Returns the collected statistics of the simulation.
        Stats& getStats();

        // Computes the arrival time of a packet at the next node based on the link characteristics.
        double compute_arrival_time(const Packet& pkt, const Link& link, double now);

        // Simulates sending a packet over a link, including potential packet loss and scheduling the next event for packet arrival.
        void sendPacket(const Packet& pkt, const Link& link, double now);

        // Exports the collected statistics to a CSV file for analysis.
        void exportStatsCSV(const RunConfig& runConfig);

        bool hasEvents() const;

        std::vector<PacketTravelInfo>& getPacketsInTransit();

        void removePacketInTransit(double departure_time, double arrival_time);

        void setGlobalLossProb(float value);

        void setGlobalPacketSize(float value);

        void setLatencyObserver(std::function<void(double)> observer);

        void notifyLatencyDelivered(double latency);

        int getGlobalPacketSize() const;

        void startTCPConnection(int source, int dest);

        void setPacketObserver(
            std::function<void(const Packet&, int from, int to, double time)> observer
        );

        void emitPacketEvent(const Packet& p, int from, int to);
    };

}