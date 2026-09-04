#include "engine/events/TCPRetransmissionEvent.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPTimeoutEvent.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

#include <algorithm>
#include <memory>

namespace kns {

    TCPRetransmissionEvent::TCPRetransmissionEvent(
        double timestamp,
        std::uint64_t session_id,
        std::uint32_t sequence
    )
        : Event(timestamp),
          session_id_(session_id),
          sequence_(sequence)
    {
    }

    void TCPRetransmissionEvent::execute(
        SimulationEngine& engine
    )
    {
        if (!engine.hasTCPSession(session_id_)) {
            return;
        }

        auto& session =
            engine.getTCPSession(session_id_);

        auto& client =
            session.getClientConnection();

        const auto segment =
            client.getOutstandingSegment(sequence_);

        if (!segment.has_value()) {
            return;
        }

        if (!client.isEstablished()) {
            return;
        }

        if (!client.canRetransmit(sequence_)) {
            client.failRetransmission();
            return;
        }

        if (!client.canRetransmit(sequence_)) {
            KNS_DEBUG_LOG(
                "[TCP][RTO] retransmission limit reached "
                << "session=" << session_id_
                << " seq=" << sequence_
                << " time=" << engine.now()
                << '\n'
            );

            return;
        }

        if (!client.isEstablished()) {
            return;
        }

        client.markSegmentRetransmitted(
            sequence_,
            engine.now()
        );

        Packet retransmission(
            client.getLocalNode(),
            client.getRemoteNode(),
            client.getLocalNode(),
            engine.now(),
            static_cast<int>(
                segment->payloadSize()
            ),
            session_id_
        );

        retransmission.packet_type =
            PacketType::DATA;

        retransmission.tcp =
            *segment;

        retransmission.tcp.window =
            static_cast<std::uint16_t>(
                std::min<std::uint32_t>(
                    client.getSendWindow(),
                    65535U
                )
            );

        retransmission.departure_time =
            engine.now();

        KNS_DEBUG_LOG(
            "[TCP][RTO] retransmit "
            << "session=" << session_id_
            << " seq=" << sequence_
            << " time=" << engine.now()
            << " rto=" << client.getCurrentRTO()
            << '\n'
        );

        const bool accepted =
            PacketUtils::sendPacketThroughTopology(
                engine,
                retransmission
            );

        if (!accepted) {
            return;
        }

        if (!client.hasOutstandingSegment(sequence_)) {
            return;
        }

        engine.schedule(
            std::make_unique<TCPTimeoutEvent>(
                engine.now() +
                    client.getCurrentRTO(),
                session_id_,
                sequence_
            )
        );
    }

} // namespace kns