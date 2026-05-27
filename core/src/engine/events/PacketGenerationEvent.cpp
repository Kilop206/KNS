#include "engine/events/PacketGenerationEvent.hpp"
#include "network/utils/PacketUtils.hpp"

#include <cstdint>

namespace kns {

    PacketGenerationEvent::PacketGenerationEvent(
        double timestamp,
        int source,
        int destination,
        PacketType type
    )
        : Event(timestamp),
        source_(source),
        destination_(destination),
        type_(type) {}

    void PacketGenerationEvent::execute(SimulationEngine& engine, uint64_t session_id) {
        Packet pkt(
            session_id,
            source_,
            destination_,
            source_,
            engine.now(),
            engine.getGlobalPacketSize()
        );

        pkt.packet_type = type_;
        pkt.seq_num = 0;
        pkt.ack_num = 0;
        pkt.departure_time = engine.now();

        sendPacketThroughTopology(engine, pkt);
    }

}