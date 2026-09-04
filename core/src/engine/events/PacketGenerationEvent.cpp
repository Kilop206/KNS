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
        if (!engine.hasTCPSession(session_id_)) {
            return;
        }

        auto& session = engine.getTCPSession(session_id_);
        auto& client = session.getClientConnection();

        if (session.getState() != TCPState::ESTABLISHED) {
            return;
        }

        if (session.isComplete()) {
            return;
        }

        Packet pkt(
            source_,
            destination_,
            source_,
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        const std::size_t payload_size =
            pkt.packet_size_bytes > 0
                ? static_cast<std::size_t>(pkt.packet_size_bytes)
                : 1;

        pkt.packet_type = PacketType::DATA;
        pkt.tcp.seq = client.getSeqNum();
        pkt.tcp.ack = client.getExpectedAckNum();
        pkt.tcp.window = 0;
        pkt.tcp.flags = TCPFlag::ACK | TCPFlag::PSH;
        pkt.tcp.payload.assign(payload_size, 0x41);
        pkt.departure_time = engine.now();

        const bool accepted =
            PacketUtils::sendPacketThroughTopology(engine, pkt);

        if (!accepted) {
            return;
        }

        client.setSeqNum(
            client.getSeqNum() +
            static_cast<std::uint32_t>(payload_size)
        );

        session.incrementPacketsSent();

        if (!session.isComplete()) {
            engine.schedule(
                std::make_unique<PacketGenerationEvent>(
                    engine.now(),
                    source_,
                    destination_,
                    session_id_
                )
            );
        }
    }
} // namespace kns