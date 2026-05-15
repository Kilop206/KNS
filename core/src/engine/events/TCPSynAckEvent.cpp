#include "engine/events/TCPSynAckEvent.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"
#include "enums/PacketType.hpp"
#include "network/utils/PacketUtils.hpp"

#include <cstdlib>

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

        Packet pkt( source_, destination_, source_, engine.now(), 1000);

        pkt.packet_type = PacketType::SYN_ACK;

        pkt.seq_num = std::rand();
        pkt.ack_num = seq_num + 1;

        sendPacketThroughTopology(engine, pkt);
    }
}
