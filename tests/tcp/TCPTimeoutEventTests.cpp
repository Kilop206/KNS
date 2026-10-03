#include <catch2/catch_test_macros.hpp>

#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPTimeoutEvent.hpp"
#include "network/Topology.hpp"
#include "network/transport/tcp/TCPSegment.hpp"

using kns::SimulationEngine;
using kns::TCPConnection;
using kns::TCPFlag;
using kns::TCPState;
using kns::TCPSegment;
using kns::TCPTimeoutEvent;
using kns::Topology;

TEST_CASE(
    "TCP timeout event is ignored after cumulative ACK",
    "[tcp][rto][timeout]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    TCPSegment segment;
    segment.seq = 1000;
    segment.payload.assign(
        100,
        0x41
    );
    segment.flags =
        TCPFlag::ACK |
        TCPFlag::PSH;

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    REQUIRE(
        connection.hasOutstandingSegment(
            1000
        )
    );

    REQUIRE(
        connection.receive_ack(
            1100,
            1.0
        )
    );

    REQUIRE_FALSE(
        connection.hasOutstandingSegment(
            1000
        )
    );
}

TEST_CASE(
    "TCP timeout increases RTO backoff",
    "[tcp][rto][timeout]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1
    );

    REQUIRE(
        connection.getCurrentRTO() == 1.0
    );

    connection.onSendTimeout();

    REQUIRE(
        connection.getCurrentRTO() == 2.0
    );

    connection.onSendTimeout();

    REQUIRE(
        connection.getCurrentRTO() == 4.0
    );
}

TEST_CASE(
    "TCP timeout event exists only for outstanding data",
    "[tcp][rto][timeout]"
)
{
    Topology topology(2);

    topology.addLink(
        0,
        1,
        10.0,
        10.0,
        0.0,
        kns::LinkMode::FULL_DUPLEX
    );

    SimulationEngine engine(topology);

    auto& session =
        engine.createTCPSession(0, 1);

    auto& client =
        session.getClientConnection();

    TCPSegment segment;

    segment.seq =
        client.getSendNext();

    segment.payload.assign(
        100,
        0x41
    );

    segment.flags =
        TCPFlag::ACK |
        TCPFlag::PSH;

    REQUIRE(
        client.queueSentSegment(
            segment,
            engine.now()
        )
    );

    REQUIRE(
        client.hasOutstandingSegment(
            segment.seq
        )
    );

    const double initial_rto =
        client.getCurrentRTO();

    TCPTimeoutEvent timeout(
        engine.now(),
        session.getSession_id(),
        segment.seq
    );

    REQUIRE_NOTHROW(
        timeout.execute(engine)
    );

    REQUIRE(
        client.getCurrentRTO() >
        initial_rto
    );
}