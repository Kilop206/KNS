#include "network/utils/PacketUtils.hpp"

namespace kns {
    bool sendPacketThroughTopology(SimulationEngine& engine, Packet& pkt) {
        int next = engine.getNextHop(pkt.current_node, pkt.destination);

        if (next == -1) {
            return false;
        }

        auto& links = engine.getTopology().getLinksFromNode(pkt.current_node);

        const Link* selected_link = nullptr;

        for (const Link& link : links)
        {
            if (link.getOtherNode(pkt.current_node) == next)
            {
                selected_link = &link;
                break;
            }
        }

        if (!selected_link)
        {
            return false;
        }

        engine.sendPacket(
            pkt,
            *selected_link,
            engine.now()
        );

        return true;
    }
}