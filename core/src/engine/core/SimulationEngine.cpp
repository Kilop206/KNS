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
#include "engine/events/Event.hpp"
#include "engine/events/PacketReceivedEvent.hpp"
#include "network/Packet.hpp"

namespace kns {

    // Generates a random number between 0 and 1 using the standard library's rand function. This method is used to simulate random events in the network, such as packet loss, by comparing the generated random number to a predefined loss probability.
    double SimulationEngine::random() {
        return (double)rand() / RAND_MAX;
    }

    // Getter for the packet loss probability
    double SimulationEngine::get_loss_prob() const {
        return loss_prob;
    }

    // Constructor that initializes the simulation engine with a given topology
    SimulationEngine::SimulationEngine(const Topology& topology)
    : topology_(topology)
    {
        int n = topology_.size();

        routing_tables_.resize(n);

        Routing routing;

        for (int u = 0; u < n; ++u) {
            routing_tables_[u] = routing.buildRoutingTable(topology_, u);
        }
    }

    // Schedule a new event in the simulation
    void SimulationEngine::schedule(std::unique_ptr<Event> event) {
        event_queue_.schedule(std::move(event));
    }

    // Get the current simulation time
    double SimulationEngine::now() const {
        return clock_.now();
    }

    // Get the topology of the network
    const Topology& SimulationEngine::getTopology() const {
        return topology_;
    }

    // Get the collected statistics of the simulation
    Stats& SimulationEngine::getStats() {
        return stats_;
    }

    // Get the next hop from the current node to the destination
    int SimulationEngine::getNextHop(int current, int destination) const {
        return routing_tables_[current][destination].next_hop;
    }

    // Run the simulation until there are no more events to process
    void SimulationEngine::run() {
        while (event_queue_.hasEvents()) {

            auto event = event_queue_.next();

            clock_.setTime(event->getTimestamp());

            event->execute(*this);
        }
    }

    void SimulationEngine::processEvent() {
        if (event_queue_.hasEvents()) {
            auto event = event_queue_.next();

            clock_.setTime(event->getTimestamp());

            event->execute(*this);
        }
    }

    double SimulationEngine::peekNextEventTime() const {
        return event_queue_.peekTimestamp();
    }

    // Helper function to compute the arrival time of a packet at the next node
    double SimulationEngine::compute_arrival_time(const Packet& pkt, const Link& link, double now) {

        // Transmission time = packet size (bits) / bandwidth (bits per second)
        double transmission =
            (pkt.packet_size_bytes * 8.0) / (link.bandwidth_mbps * 1e6);
        
        // Propagation time = link delay (ms) converted to seconds
        double propagation = link.delay_ms / 1000.0;

        // Total time = propagation + transmission
        return (now + propagation + transmission);
    }

    // Send a packet over a link, simulating potential loss and scheduling the next event
    void SimulationEngine::sendPacket(const Packet& pkt, const Link& link, double now) {
        stats_.packets_sent++;

        // Drop
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

        // Compute arrival
        double arrival_time = compute_arrival_time(pkt, link, now);

        int next_node = link.getOtherNode(pkt.current_node);

        Packet new_pkt = pkt;
        new_pkt.current_node = next_node;
        new_pkt.hop_count++;
        new_pkt.departure_time = now;

        auto event = std::make_unique<PacketReceivedEvent>(
            arrival_time,
            new_pkt
        );

        packets_in_transit.push_back(PacketTravelInfo(now, arrival_time, link.from, link.to));

        // Optional debug trace.
        // std::cout << "[SENT] Packet from " << pkt.source << " to " << pkt.destination << "\n";

        event_queue_.schedule(std::move(event));
    }

    void SimulationEngine::exportStatsCSV(const RunConfig& runConfig) {
        // Append to the file if it exists, otherwise create a new one
        std::ofstream file(runConfig.filename, std::ios::out);

        // Writes the header
        file << "Packets Sent,Packets Delivered,Packets Lost,Delivery Rate,Loss Rate,Average Latency,Seed\n";

        // Calculate rates and average latency
        double delivery_rate = stats_.packets_sent > 0
            ? (double)stats_.packets_delivered / stats_.packets_sent
            : 0.0;

        double loss_rate = stats_.packets_sent > 0
            ? (double)stats_.packets_lost / stats_.packets_sent
            : 0.0;
        double avg_latency = stats_.packets_delivered > 0 
                            ? stats_.total_latency / stats_.packets_delivered
                            : 0.0;

        // Write the stats to the CSV file
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

    std::vector<PacketTravelInfo>& SimulationEngine::getPacketsInTransit() {
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

    void SimulationEngine::setGlobalPacketSize(float value) {
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
}