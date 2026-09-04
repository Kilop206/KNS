#include "engine/events/TCPTimeoutEvent.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

#include <memory>
#include <algorithm>

namespace kns {

    TCPTimeoutEvent::TCPTimeoutEvent(
        double timestamp,
        std::uint64_t session_id,
        std::uint32_t sequence
    )
        : Event(timestamp),
          session_id_(session_id),
          sequence_(sequence)
    {
    }

    void TCPTimeoutEvent::execute(
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

        const auto oldest =
            client.getOldestOutstandingSequence();

        /*
        * Only the timer associated with the oldest
        * unacknowledged segment is authoritative.
        *
        * Older logical timer events can remain in the
        * EventQueue because the queue has no physical
        * cancellation mechanism.
        */
        if (
            !oldest.has_value() ||
            *oldest != sequence_
        ) {
            return;
        }

        client.onSendTimeout();

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

        PacketUtils::sendPacketThroughTopology(
            engine,
            retransmission
        );

        /*
        * The segment is still outstanding, therefore its
        * timer must continue to exist.
        *
        * If the segment is ACKed before this event executes,
        * getOutstandingSegment() will return nullopt and
        * the event becomes a no-op.
        */
        if (client.hasOutstandingSegment(sequence_)) {
            engine.schedule(
                std::make_unique<TCPTimeoutEvent>(
                    engine.now() +
                        client.getCurrentRTO(),
                    session_id_,
                    sequence_
                )
            );
        }
    }

} // namespace kns