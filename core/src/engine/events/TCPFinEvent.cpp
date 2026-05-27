#include "engine/events/TCPFinEvent.hpp"
#include "network/Packet.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns {
    TCPFinEvent::TCPFinEvent(double timestamp,
            int source,
            int destination,
            uint32_t seq_num,
            uint32_t ack_num,
            uint64_t session_id) :
            Event(timestamp),
            source_(source),
            destination_(destination),
            seq_num(seq_num),
            ack_num(ack_num),
            session_id(session_id) {}

    void TCPFinEvent::execute(SimulationEngine& engine) {

        Packet pkt(
            source_,
            destination_,
            source_,
            engine.now(),
            1000,
            session_id
        );

        pkt.packet_type = PacketType::FIN;

        pkt.seq_num = seq_num;
        pkt.ack_num = ack_num;

        sendPacketThroughTopology(engine, pkt);
    }
}
