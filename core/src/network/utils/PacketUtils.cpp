#include "network/utils/PacketUtils.hpp"

#include <iostream>

namespace kns {

bool sendPacketThroughTopology(SimulationEngine& engine, Packet pkt) {
    const int next = engine.getNextHop(pkt.current_node, pkt.destination);

    if (next == -1) {
        engine.getStats().packets_lost++;
        std::cout << "[DROPPED] Packet from " << pkt.source
                  << " to " << pkt.destination
                  << " at time " << engine.now() << '\n';
        return false;
    }

    const auto& links = engine.getTopology().getLinksFromNode(pkt.current_node);

    const Link* selected_link = nullptr;
    for (const Link& link : links) {
        if (link.getOtherNode(pkt.current_node) == next) {
            selected_link = &link;
            break;
        }
    }

    if (!selected_link) {
        engine.getStats().packets_lost++;
        std::cout << "[DROPPED] Packet from " << pkt.source
                  << " to " << pkt.destination
                  << " at time " << engine.now() << '\n';
        return false;
    }

    engine.sendPacket(pkt, *selected_link, engine.now());
    return true;
}

}