#include "engine/events/TCPAckEvent.hpp"
#include "network/Packet.hpp"
#include "network/utils/PacketUtils.hpp"

#include <iostream>

namespace kns {
    TCPAckEvent::TCPAckEvent(double timestamp,
            int source,
            int destination,
            uint32_t seq_num,
            uint32_t ack_num) :
            Event(timestamp),
            source_(source),
            destination_(destination),
            seq_num(seq_num),
            ack_num(ack_num) {}

    void TCPAckEvent::execute(SimulationEngine& engine) {

        Packet pkt(destination_, source_, destination_, engine.now(), 1000);

        pkt.packet_type = PacketType::ACK;

        pkt.seq_num = seq_num;
        pkt.ack_num = ack_num;

        sendPacketThroughTopology(engine, pkt);
    }
}
