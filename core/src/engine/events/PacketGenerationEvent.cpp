#include "engine/events/PacketGenerationEvent.hpp"

#include "network/Packet.hpp"
#include "network/utils/PacketUtils.hpp"

#include <cstdint>
#include <iostream>

namespace kns {

    PacketGenerationEvent::PacketGenerationEvent(
        double timestamp,
        int source,
        int destination,
        std::uint64_t session_id
    )
        : Event(timestamp),
          source_(source),
          destination_(destination),
          session_id_(session_id)
    {
    }

    void PacketGenerationEvent::execute(SimulationEngine& engine)
    {
        Packet pkt(
            source_,
            destination_,
            source_,
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        pkt.packet_type = PacketType::DATA;
        pkt.tcp.seq = 0;
        pkt.tcp.ack = 0;
        pkt.tcp.window = 0;
        pkt.tcp.flags = TCPFlag::ACK | TCPFlag::PSH;
        pkt.tcp.payload.assign(1, 0x41);
        pkt.departure_time = engine.now();

        PacketUtils::sendPacketThroughTopology(engine, pkt);
        
        engine.getStats().packets_sent++;
    }

}