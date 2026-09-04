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

        if (pkt.current_node < 0 || pkt.current_node >= engine.getTopology().size()) {
            return false;
        }

        const auto& links = engine.getTopology().getLinksFromNode(pkt.current_node);

        Link* selected_link = nullptr;

        for (const auto& link_ptr : links) {
            if (link_ptr && link_ptr->isUp() && link_ptr->getOtherNode(pkt.current_node) == next) {
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

        // If the previous node is still within topology bounds, attempt to dequeue
        // from the link's FIFO queue using its stable link_id.
        if (pkt.previous_node < engine.getTopology().size()) {
            const auto& links =
                engine.getTopology().getLinksFromNode(pkt.previous_node);

            for (const auto& link_ptr : links) {
                if (link_ptr && link_ptr->getId() == pkt.link_id) {
                    link_ptr->dequeueTransmission(
                        pkt.previous_node,
                        pkt.current_node,
                        pkt.departure_time,
                        pkt.arrival_time
                    );
                    break;
                }
            }
        }

        // Always remove the packet from packets_in_transit, even if the link
        // or node was removed from the topology while the packet was in transit (issue #95).
        engine.removePacketInTransit(
            pkt.departure_time,
            pkt.arrival_time,
            pkt.previous_node,
            pkt.current_node,
            pkt.link_id
        );

        return true;
    }
}