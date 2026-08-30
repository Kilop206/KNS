#include "network/utils/PacketUtils.hpp"

namespace kns {

    bool PacketUtils::sendPacketThroughTopology(
        SimulationEngine& engine,
        const Packet& pkt
    ) {
        const int next = engine.getNextHop(pkt.current_node, pkt.destination);

        if (next == -1) {
            return false;
        }

        const auto& links = engine.getTopology().getLinksFromNode(pkt.current_node);

        Link* selected_link = nullptr;

        for (const auto& link_ptr : links) {
            if (link_ptr && link_ptr->getOtherNode(pkt.current_node) == next) {
                selected_link = link_ptr.get();
                break;
            }
        }

        if (!selected_link) {
            return false;
        }

        return engine.sendPacket(pkt, *selected_link, engine.now());
    }

    bool PacketUtils::releasePacketThroughTopology(
        SimulationEngine& engine,
        const Packet& pkt
    ) {
        if (pkt.previous_node < 0) {
            return false;
        }

        // Use the stable link_id recorded at transmission time so that the
        // correct link is released even when multiple parallel links exist
        // between the same pair of nodes (fixes issue #92).
        const auto& links =
            engine.getTopology().getLinksFromNode(pkt.previous_node);

        for (const auto& link_ptr : links) {
            if (link_ptr && link_ptr->getId() == pkt.link_id) {
                link_ptr->dequeuePacket();
                return true;
            }
        }

        return false;
    }
}