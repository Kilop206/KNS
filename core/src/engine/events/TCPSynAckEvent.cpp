#include "engine/events/TCPSynAckEvent.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"
#include "enums/PacketType.hpp"
#include "network/utils/PacketUtils.hpp"
#include "engine/events/TCPAckEvent.hpp"

#include <cstdlib>
#include <iostream>

namespace kns {
    TCPSynAckEvent::TCPSynAckEvent(double timestamp,
            int source,
            int destination,
            uint32_t seq_num,
            uint32_t ack_num) :
        Event(timestamp),
        source_(source),
        destination_(destination),
        seq_num(seq_num),
        ack_num(ack_num) {}

    void TCPSynAckEvent::execute(SimulationEngine& engine) {

        Packet pkt( destination_, source_, destination_, engine.now(), 1000);

        pkt.packet_type = PacketType::SYN_ACK;

        pkt.seq_num = std::rand();
        pkt.ack_num = seq_num + 1;

        sendPacketThroughTopology(engine, pkt);

        double timestamp;

        for (Link link : engine.getTopology().getLinksFromNode(destination_)) {
            if (link.getOtherNode(destination_) == engine.getNextHop(destination_, source_)) {
                timestamp = engine.compute_arrival_time(pkt, link, engine.now());
                break;
            }
        }

        engine.schedule(std::make_unique<TCPAckEvent>(timestamp, source_, destination_, pkt.seq_num, pkt.ack_num));
    }
}
