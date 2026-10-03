#include "engine/events/TCPFastRetransmitEvent.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPTimeoutEvent.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

#include <algorithm>
#include <memory>

namespace kns {

    TCPFastRetransmitEvent::TCPFastRetransmitEvent(
        double timestamp,
        std::uint64_t session_id
    )
        : Event(timestamp),
          session_id_(session_id)
    {
    }

    void TCPFastRetransmitEvent::execute(
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

        if (!client.isEstablished()) {
            return;
        }

        const auto oldest =
            client.getOldestOutstandingSequence();

        if (!oldest.has_value()) {
            return;
        }

        const std::uint32_t sequence =
            *oldest;

        const auto segment =
            client.getOutstandingSegment(sequence);

        if (!segment.has_value()) {
            return;
        }

        if (!client.canRetransmit(sequence)) {
            KNS_DEBUG_LOG(
                "[TCP][FAST RETRANSMIT] "
                << "retransmission limit reached "
                << "session=" << session_id_
                << " seq=" << sequence
                << " time=" << engine.now()
                << '\n'
            );

            client.failRetransmission();
            return;
        }

        const std::uint32_t flight_size =
            client.getSendNext() -
            client.getSendUnacknowledged();

        client.getCongestionControl().onFastRetransmit(
            flight_size
        );

        client.recordCongestionSample(
            engine.now()
        );

        client.markSegmentRetransmitted(
            sequence,
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
                    client.getReceiveWindow(),
                    65535U
                )
            );

        retransmission.departure_time =
            engine.now();

        KNS_DEBUG_LOG(
            "[TCP][FAST RETRANSMIT] "
            << "retransmit "
            << "session=" << session_id_
            << " seq=" << sequence
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

        if (!client.hasOutstandingSegment(sequence)) {
            return;
        }

        engine.schedule(
            std::make_unique<TCPTimeoutEvent>(
                engine.now() +
                    client.getCurrentRTO(),
                session_id_,
                sequence
            )
        );
    }

} // namespace kns
