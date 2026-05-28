#include "engine/events/PacketGenerationEvent.hpp"
#include "network/utils/PacketUtils.hpp"

#include <cstdint>

namespace kns {

    PacketGenerationEvent::PacketGenerationEvent(
        double timestamp,
        int source,
        int destination,
        uint64_t session_id)
        :
        Event(timestamp),
        source_(source),
        destination_(destination),
        session_id_(session_id) {}

    void PacketGenerationEvent::execute(SimulationEngine& engine) {
        Packet pkt(
            session_id_,
            source_,
            destination_,
            source_,
            engine.now(),
            engine.getGlobalPacketSize()
        );

        pkt.seq_num = 0;
        pkt.ack_num = 0;
        pkt.departure_time = engine.now();

        sendPacketThroughTopology(engine, pkt);
    }

}