#include <catch2/catch_test_macros.hpp>

#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/transport/tcp/TCPSession.hpp"

using kns::SimulationEngine;
using kns::TCPState;

TEST_CASE(
    "TCP send window blocks data when full",
    "[tcp][window][integration]"
)
{
    Topology topology(2);

    topology.addLink(
        0,
        1,
        10.0,
        10.0,
        0.0,
        LinkMode::FULL_DUPLEX
    );

    SimulationEngine engine(topology);

    auto& session = engine.createTCPSession(0, 1);
    auto& client = session.getClientConnection();

    client.setSendWindow(100);

    const auto initial_seq = client.getSendNext();

    REQUIRE(
        client.getSendBufferSize() == 0
    );

    TCPSegment first;
    first.seq = initial_seq;
    first.payload.assign(100, 0x41);
    first.flags = TCPFlag::ACK | TCPFlag::PSH;

    REQUIRE(
        client.queueSentSegment(
            first,
            engine.now()
        )
    );

    REQUIRE(
        client.getSendNext() ==
        initial_seq + 100
    );

    REQUIRE(
        client.getSendUnacknowledged() ==
        initial_seq
    );

    REQUIRE_FALSE(
        client.canSend(1)
    );
}

TEST_CASE(
    "TCP cumulative ACK reopens send window",
    "[tcp][window][integration]"
)
{
    Topology topology(2);

    topology.addLink(
        0,
        1,
        10.0,
        10.0,
        0.0,
        LinkMode::FULL_DUPLEX
    );

    SimulationEngine engine(topology);

    auto& session = engine.createTCPSession(0, 1);
    auto& client = session.getClientConnection();

    client.setSendWindow(200);

    const auto initial_seq = client.getSendNext();

    TCPSegment first;
    first.seq = initial_seq;
    first.payload.assign(100, 0x41);
    first.flags = TCPFlag::ACK | TCPFlag::PSH;

    TCPSegment second;
    second.seq = initial_seq + 100;
    second.payload.assign(100, 0x42);
    second.flags = TCPFlag::ACK | TCPFlag::PSH;

    REQUIRE(
        client.queueSentSegment(
            first,
            engine.now()
        )
    );

    REQUIRE(
        client.queueSentSegment(
            second,
            engine.now()
        )
    );

    REQUIRE_FALSE(
        client.canSend(1)
    );

    REQUIRE(
        client.receive_ack(initial_seq + 100)
    );

    REQUIRE(
        client.getSendBufferSize() == 1
    );

    REQUIRE(
        client.canSend(100)
    );
}