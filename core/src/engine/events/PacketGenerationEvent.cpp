#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/core/SimulationEngine.hpp"

#include <iostream>

namespace kns {

PacketGenerationEvent::PacketGenerationEvent(
    double timestamp,
    int source,
    int destination
)
    : Event(timestamp),
      source_(source),
      destination_(destination)
{}

void PacketGenerationEvent::execute(SimulationEngine& engine) {

    // Create a new packet with the specified source, destination, and size. The current node is initialized to the source, and the creation time is set to the current simulation time.
    Packet pkt(
        source_,
        destination_,
        source_,
        engine.now(),
        engine.getGlobalPacketSize()
    );

    pkt.departure_time = engine.now();

    // Get the next hop for the packet from the source to the destination. This is done by querying the routing table in the simulation engine, which provides the next node that the packet should be forwarded to in order to reach its destination. If there is no valid next hop (i.e., if getNextHop returns -1), it means that there is no route from the source to the destination, and we can simply return without scheduling any further events, as the packet cannot be forwarded.
    int next = engine.getNextHop(source_, destination_);
    if (next == -1) {
        engine.getStats().packets_lost++;
        std::cout << "[DROPPED] Packet from " << pkt.source
                  << " to " << pkt.destination
                  << " at time " << engine.now() << '\n';
        return;
    }

    const auto& links = engine.getTopology().getLinksFromNode(source_);

    const Link* selected_link = nullptr;

    // Find the link to the next hop
    for (const Link& link : links) {
        if (link.getOtherNode(source_) == next) {
            selected_link = &link;
            break;
        }
    }

    if (!selected_link) {
        engine.getStats().packets_lost++;
        std::cout << "[DROPPED] Packet from " << pkt.source
                  << " to " << pkt.destination
                  << " at time " << engine.now() << '\n';
        return;
    }

    // Schedule the packet to be sent over the selected link. This involves calculating the arrival time of the packet at the next node based on the properties of the link (such as delay and bandwidth) and then scheduling a new PacketReceivedEvent for the next node at that calculated arrival time. The sendPacket method of the simulation engine is responsible for handling the logic of sending the packet, including updating its current node and scheduling the appropriate events.
    engine.sendPacket(pkt, *selected_link, engine.now());
}

}