#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "engine/core/SimulationEngine.hpp"
#include "engine/events/PacketReceivedEvent.hpp"
#include "engine/events/TCPFastRetransmitEvent.hpp"
#include "network/Link.hpp"
#include "network/Packet.hpp"
#include "network/Topology.hpp"
#include "network/transport/tcp/TCPSegment.hpp"
#include "network/transport/tcp/TCPSession.hpp"

using kns::LinkMode;
using kns::Packet;
using kns::PacketReceivedEvent;
using kns::SimulationEngine;
using kns::TCPConnection;
using kns::TCPFastRetransmitEvent;
using kns::TCPSegment;
using kns::TCPSession;
using kns::TCPFlag;
using kns::TCPState;
using kns::Topology;

namespace
{
    void establishSession(TCPSession& session)
    {
        auto& client =
            session.getClientConnection();

        auto& server =
            session.getServerConnection();

        REQUIRE(
            client.getTcpState() ==
            TCPState::CLOSED
        );

        REQUIRE(
            server.getTcpState() ==
            TCPState::CLOSED
        );

        REQUIRE(
            client.send_syn()
        );

        const auto client_syn_sequence =
            client.getSeqNum();

        REQUIRE(
            server.receive_syn(
                client_syn_sequence
            )
        );

        REQUIRE(
            server.getTcpState() ==
            TCPState::SYN_RECEIVED
        );

        const auto syn_ack =
            server.buildSynAck();

        REQUIRE(
            client.receive_syn_ack(
                syn_ack.seq,
                syn_ack.ack
            )
        );

        REQUIRE(
            client.getTcpState() ==
            TCPState::ESTABLISHED
        );

        const auto ack =
            client.buildAck();

        REQUIRE(
            server.receive_ack(
                ack.ack,
                0.0
            )
        );

        REQUIRE(
            server.getTcpState() ==
            TCPState::ESTABLISHED
        );

        session.markTrafficGenerated();
    }

    TCPSegment makeOutstandingSegment(
        std::uint32_t sequence
    )
    {
        TCPSegment segment;

        segment.seq = sequence;
        segment.ack = 0;

        segment.window =
            static_cast<std::uint16_t>(
                TCPConnection::DEFAULT_SEND_WINDOW
            );

        segment.flags =
            TCPFlag::ACK |
            TCPFlag::PSH;

        segment.payload.assign(
            100,
            0x41
        );

        return segment;
    }

    Packet makeDuplicateAck(
        std::uint64_t session_id,
        std::uint32_t acknowledgement
    )
    {
        Packet packet(
            1,
            0,
            0,
            0.0,
            0,
            session_id
        );

        packet.packet_type =
            kns::PacketType::ACK;

        packet.tcp.flags =
            TCPFlag::ACK;

        packet.tcp.ack =
            acknowledgement;

        return packet;
    }
}

TEST_CASE(
    "TCP duplicate ACK threshold schedules fast retransmit",
    "[tcp][fast-retransmit][integration]"
)
{
    Topology topology(2);

    auto link =
        topology.addLinkPtr(
            0,
            1,
            100.0,
            100.0,
            0.0,
            LinkMode::FULL_DUPLEX
        );

    REQUIRE(link != nullptr);

    SimulationEngine engine(topology);

    auto& session =
        engine.createTCPSession(
            0,
            1
        );

    establishSession(session);

    auto& client =
        session.getClientConnection();

    TCPSegment segment =
        makeOutstandingSegment(
            client.getSendNext()
        );

    REQUIRE(
        client.queueSentSegment(
            segment,
            engine.now()
        )
    );

    const auto duplicate_ack =
        client.getSendUnacknowledged();

    REQUIRE(
        client.getDuplicateAckCount() ==
        0
    );

    PacketReceivedEvent first(
        engine.now(),
        makeDuplicateAck(
            session.getSession_id(),
            duplicate_ack
        )
    );

    first.execute(engine);

    REQUIRE(
        client.getDuplicateAckCount() ==
        1
    );

    REQUIRE_FALSE(
        client.shouldFastRetransmit()
    );

    PacketReceivedEvent second(
        engine.now(),
        makeDuplicateAck(
            session.getSession_id(),
            duplicate_ack
        )
    );

    second.execute(engine);

    REQUIRE(
        client.getDuplicateAckCount() ==
        2
    );

    REQUIRE_FALSE(
        client.shouldFastRetransmit()
    );

    PacketReceivedEvent third(
        engine.now(),
        makeDuplicateAck(
            session.getSession_id(),
            duplicate_ack
        )
    );

    third.execute(engine);

    REQUIRE(
        client.getDuplicateAckCount() ==
        0
    );

    REQUIRE_FALSE(
        client.shouldFastRetransmit()
    );

    REQUIRE(
        engine.hasEvents()
    );
}

TEST_CASE(
    "TCP fast retransmit retransmits oldest outstanding segment",
    "[tcp][fast-retransmit]"
)
{
    Topology topology(2);

    auto link =
        topology.addLinkPtr(
            0,
            1,
            100.0,
            100.0,
            0.0,
            LinkMode::FULL_DUPLEX
        );

    REQUIRE(link != nullptr);

    SimulationEngine engine(topology);

    auto& session =
        engine.createTCPSession(
            0,
            1
        );

    establishSession(session);

    auto& client =
        session.getClientConnection();

    TCPSegment first =
        makeOutstandingSegment(
            client.getSendNext()
        );

    REQUIRE(
        client.queueSentSegment(
            first,
            engine.now()
        )
    );

    TCPSegment second =
        makeOutstandingSegment(
            client.getSendNext()
        );

    REQUIRE(
        client.queueSentSegment(
            second,
            engine.now()
        )
    );

    const auto oldest =
        client.getOldestOutstandingSequence();

    REQUIRE(oldest.has_value());

    REQUIRE(
        *oldest ==
        first.seq
    );

    REQUIRE(
        client.getRetransmissionCount(
            first.seq
        ) == 0
    );

    REQUIRE(
        client.getRetransmissionCount(
            second.seq
        ) == 0
    );

    TCPFastRetransmitEvent event(
        engine.now(),
        session.getSession_id()
    );

    event.execute(engine);

    REQUIRE(
        client.getRetransmissionCount(
            first.seq
        ) == 1
    );

    REQUIRE(
        client.hasOutstandingSegment(
            first.seq
        )
    );

    REQUIRE(
        client.getRetransmissionCount(
            second.seq
        ) == 0
    );

    REQUIRE(
        client.getSendUnacknowledged() ==
        first.seq
    );
}

TEST_CASE(
    "TCP fast retransmit schedules timeout for retransmitted segment",
    "[tcp][fast-retransmit][timeout]"
)
{
    Topology topology(2);

    /*
     * Use a delay larger than the default RTO so that the
     * retransmitted packet's arrival event is scheduled
     * after the timeout event.
     */
    auto link =
        topology.addLinkPtr(
            0,
            1,
            100.0,
            2000.0,
            0.0,
            LinkMode::FULL_DUPLEX
        );

    REQUIRE(link != nullptr);

    SimulationEngine engine(topology);

    auto& session =
        engine.createTCPSession(
            0,
            1
        );

    establishSession(session);

    auto& client =
        session.getClientConnection();

    TCPSegment segment =
        makeOutstandingSegment(
            client.getSendNext()
        );

    REQUIRE(
        client.queueSentSegment(
            segment,
            engine.now()
        )
    );

    const auto sequence =
        segment.seq;

    const double retransmit_time =
        engine.now();

    const double expected_timeout =
        retransmit_time +
        client.getCurrentRTO();

    TCPFastRetransmitEvent event(
        retransmit_time,
        session.getSession_id()
    );

    event.execute(engine);

    REQUIRE(
        client.getRetransmissionCount(
            sequence
        ) == 1
    );

    REQUIRE(
        client.hasOutstandingSegment(
            sequence
        )
    );

    REQUIRE(
        engine.hasEvents()
    );

    REQUIRE(
        engine.peekNextEventTime() ==
        expected_timeout
    );
}

TEST_CASE(
    "TCP ACK advancement clears duplicate ACK loss condition",
    "[tcp][fast-retransmit][integration]"
)
{
    Topology topology(2);

    auto link =
        topology.addLinkPtr(
            0,
            1,
            100.0,
            100.0,
            0.0,
            LinkMode::FULL_DUPLEX
        );

    REQUIRE(link != nullptr);

    SimulationEngine engine(topology);

    auto& session =
        engine.createTCPSession(
            0,
            1
        );

    establishSession(session);

    auto& client =
        session.getClientConnection();

    TCPSegment segment =
        makeOutstandingSegment(
            client.getSendNext()
        );

    REQUIRE(
        client.queueSentSegment(
            segment,
            engine.now()
        )
    );

    const auto duplicate_ack =
        client.getSendUnacknowledged();

    REQUIRE_FALSE(
        client.receive_ack(
            duplicate_ack,
            engine.now()
        )
    );

    REQUIRE_FALSE(
        client.receive_ack(
            duplicate_ack,
            engine.now()
        )
    );

    REQUIRE(
        client.getDuplicateAckCount() ==
        2
    );

    const auto advancing_ack =
        segment.seq +
        static_cast<std::uint32_t>(
            segment.payloadSize()
        );

    REQUIRE(
        client.receive_ack(
            advancing_ack,
            engine.now()
        )
    );

    REQUIRE(
        client.getDuplicateAckCount() ==
        0
    );

    REQUIRE_FALSE(
        client.shouldFastRetransmit()
    );

    REQUIRE(
        client.getSendUnacknowledged() ==
        advancing_ack
    );
}

TEST_CASE(
    "TCP retransmission limit remains enforced by fast retransmit",
    "[tcp][fast-retransmit][retransmission]"
)
{
    Topology topology(2);

    auto link =
        topology.addLinkPtr(
            0,
            1,
            100.0,
            100.0,
            0.0,
            LinkMode::FULL_DUPLEX
        );

    REQUIRE(link != nullptr);

    SimulationEngine engine(topology);

    auto& session =
        engine.createTCPSession(
            0,
            1
        );

    establishSession(session);

    auto& client =
        session.getClientConnection();

    TCPSegment segment =
        makeOutstandingSegment(
            client.getSendNext()
        );

    REQUIRE(
        client.queueSentSegment(
            segment,
            engine.now()
        )
    );

    const auto sequence =
        segment.seq;

    /*
     * Bring the segment exactly to the retransmission limit.
     * The next fast retransmit must therefore be rejected.
     */
    for (
        std::uint32_t i = 0;
        i <
        TCPConnection::MAX_DATA_RETRANSMISSIONS;
        ++i
    )
    {
        REQUIRE(
            client.markSegmentRetransmitted(
                sequence,
                engine.now()
            )
        );
    }

    REQUIRE(
        client.getRetransmissionCount(
            sequence
        ) ==
        TCPConnection::MAX_DATA_RETRANSMISSIONS
    );

    REQUIRE_FALSE(
        client.canRetransmit(
            sequence
        )
    );

    REQUIRE(
        client.isEstablished()
    );

    TCPFastRetransmitEvent event(
        engine.now(),
        session.getSession_id()
    );

    event.execute(engine);

    /*
     * failRetransmission() closes the connection and clears
     * the send/receive buffers.
     */
    REQUIRE(
        client.getTcpState() ==
        TCPState::CLOSED
    );

    REQUIRE_FALSE(
        client.isEstablished()
    );

    REQUIRE(
        client.getSendBufferSize() ==
        0
    );

    REQUIRE_FALSE(
        client.hasOutstandingSegment(
            sequence
        )
    );

    REQUIRE(
        client.getRetransmissionCount(
            sequence
        ) == 0
    );
}
