#include <cassert>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <utility>

#include "engine/events/PacketReceivedEvent.hpp"
#include "engine/core/SimulationEngine.hpp"

namespace kns {

    PacketReceivedEvent::PacketReceivedEvent(double timestamp, Packet packet)
        : Event(timestamp), packet(std::move(packet)) {}

    void PacketReceivedEvent::execute(SimulationEngine& engine) {
        int u = packet.current_node;
        int dest = packet.destination;

        assert(u >= 0);

        if (u == dest) {
            auto& stats = engine.getStats();

            stats.packets_delivered++;

            const double latency = engine.now() - packet.creation_time;
            stats.total_latency += latency;

            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6)
                << "[DELIVERED] Packet from " << packet.source
                << " to " << packet.destination
                << " latency=" << latency;
            std::cout << oss.str() << '\n';

            std::cout << "[LATENCY] " << std::fixed << std::setprecision(6)
                      << latency << '\n';

            engine.notifyLatencyDelivered(latency);

            engine.removePacketInTransit(packet.departure_time, timestamp_);
            return;
        }

        int next = engine.getNextHop(u, dest);
        if (next == -1) {
            engine.getStats().packets_lost++;
            std::cout << "[DROPPED] Packet from " << packet.source
                      << " to " << packet.destination
                      << " at time " << engine.now() << '\n';
            engine.removePacketInTransit(packet.departure_time, timestamp_);
            return;
        }

        const auto& links = engine.getTopology().getLinksFromNode(u);

        const Link* selected_link = nullptr;

        for (const Link& link : links) {
            if (link.getOtherNode(u) == next) {
                selected_link = &link;
                break;
            }
        }

        if (!selected_link) {
            engine.getStats().packets_lost++;
            std::cout << "[DROPPED] Packet from " << packet.source
                      << " to " << packet.destination
                      << " at time " << engine.now() << '\n';
            engine.removePacketInTransit(packet.departure_time, timestamp_);
            return;
        }

        engine.sendPacket(packet, *selected_link, timestamp_);
        engine.removePacketInTransit(packet.departure_time, timestamp_);
    }
}