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

        /*
        * Logical timer cancellation:
        *
        * If the segment disappeared from the send buffer,
        * it was cumulatively acknowledged before this timeout
        * event reached the queue.
        */
        if (!segment.has_value()) {
            return;
        }

        /*
        * A timeout always backs off the RTO while the
        * corresponding segment remains outstanding.
        */
        client.onSendTimeout();

        /*
        * Once the connection leaves ESTABLISHED, the TCP
        * session is already entering its closing path.
        *
        * Do not create new retransmission work or new timers,
        * otherwise the event queue can remain non-empty forever
        * because the original DATA segment is retained until the
        * closing sequence finishes.
        */
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

        /*
        * Schedule another logical timer only when the
        * retransmission was actually accepted by the network.
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