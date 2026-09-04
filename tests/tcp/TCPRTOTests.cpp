#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPTimeoutEvent.hpp"
#include "network/Topology.hpp"
#include "network/transport/tcp/TCPSegment.hpp"
#include "network/transport/tcp/TCPConnection.hpp"

using kns::RTOManager;
using kns::RTTEstimator;
using kns::SimulationEngine;
using kns::TCPConnection;
using kns::TCPFlag;
using kns::TCPState;
using kns::TCPSegment;
using kns::TCPTimeoutEvent;
using kns::Topology;

namespace {

    constexpr double EPSILON = 1e-9;

    bool almostEqual(double a, double b) noexcept
    {
        return std::abs(a - b) <= EPSILON;
    }

}

TEST_CASE(
    "RTTEstimator starts with default RTO",
    "[tcp][rto]"
)
{
    RTTEstimator estimator;

    REQUIRE_FALSE(estimator.hasSample());
    REQUIRE(estimator.getSRTT() == 0.0);
    REQUIRE(estimator.getRTTVAR() == 0.0);
    REQUIRE(
        estimator.getRTO() ==
        RTTEstimator::INITIAL_RTO
    );
}

TEST_CASE(
    "RTTEstimator initializes from first RTT sample",
    "[tcp][rto]"
)
{
    RTTEstimator estimator;

    estimator.update(1.0);

    REQUIRE(estimator.hasSample());

    REQUIRE(
        almostEqual(
            estimator.getSRTT(),
            1.0
        )
    );

    REQUIRE(
        almostEqual(
            estimator.getRTTVAR(),
            0.5
        )
    );

    REQUIRE(
        almostEqual(
            estimator.getRTO(),
            3.0
        )
    );
}

TEST_CASE(
    "RTTEstimator updates SRTT and RTTVAR",
    "[tcp][rto]"
)
{
    RTTEstimator estimator;

    estimator.update(1.0);
    estimator.update(1.2);

    REQUIRE(
        almostEqual(
            estimator.getSRTT(),
            1.025
        )
    );

    REQUIRE(
        almostEqual(
            estimator.getRTTVAR(),
            0.425
        )
    );

    REQUIRE(
        almostEqual(
            estimator.getRTO(),
            2.725
        )
    );
}

TEST_CASE(
    "RTTEstimator ignores invalid RTT samples",
    "[tcp][rto]"
)
{
    RTTEstimator estimator;

    estimator.update(-1.0);

    REQUIRE_FALSE(estimator.hasSample());

    REQUIRE(
        estimator.getRTO() ==
        RTTEstimator::INITIAL_RTO
    );

    estimator.update(0.0);

    REQUIRE_FALSE(estimator.hasSample());
}

TEST_CASE(
    "RTTEstimator clamps RTO to minimum",
    "[tcp][rto]"
)
{
    RTTEstimator estimator;

    estimator.update(0.01);

    REQUIRE(
        estimator.getRTO() ==
        RTTEstimator::MIN_RTO
    );
}

TEST_CASE(
    "RTTEstimator reset restores initial state",
    "[tcp][rto]"
)
{
    RTTEstimator estimator;

    estimator.update(1.0);

    REQUIRE(estimator.hasSample());

    estimator.reset();

    REQUIRE_FALSE(estimator.hasSample());
    REQUIRE(estimator.getSRTT() == 0.0);
    REQUIRE(estimator.getRTTVAR() == 0.0);

    REQUIRE(
        estimator.getRTO() ==
        RTTEstimator::INITIAL_RTO
    );
}

TEST_CASE(
    "RTOManager applies exponential backoff",
    "[tcp][rto]"
)
{
    RTOManager manager;

    REQUIRE(
        manager.currentRTO() ==
        RTTEstimator::INITIAL_RTO
    );

    REQUIRE(
        manager.onTimeout() == 2.0
    );

    REQUIRE(
        manager.getBackoff() == 2.0
    );

    REQUIRE(
        manager.onTimeout() == 4.0
    );

    REQUIRE(
        manager.getBackoff() == 4.0
    );

    REQUIRE(
        manager.onTimeout() == 8.0
    );

    REQUIRE(
        manager.getBackoff() == 8.0
    );
}

TEST_CASE(
    "RTOManager resets backoff after acknowledgement",
    "[tcp][rto]"
)
{
    RTOManager manager;

    REQUIRE(
        manager.onTimeout() == 2.0
    );

    REQUIRE(
        manager.onTimeout() == 4.0
    );

    manager.onAcknowledgement(1.0);

    REQUIRE(
        manager.getBackoff() == 1.0
    );

    REQUIRE(
        almostEqual(
            manager.currentRTO(),
            3.0
        )
    );
}

TEST_CASE(
    "RTOManager reset clears estimator and backoff",
    "[tcp][rto]"
)
{
    RTOManager manager;

    manager.onAcknowledgement(1.0);
    manager.onTimeout();
    manager.onTimeout();

    manager.reset();

    REQUIRE(
        manager.getBackoff() == 1.0
    );

    REQUIRE_FALSE(
        manager.getEstimator().hasSample()
    );

    REQUIRE(
        manager.currentRTO() ==
        RTTEstimator::INITIAL_RTO
    );
}

TEST_CASE(
    "TCP timeout retransmits outstanding segment and applies Karn",
    "[tcp][rto][timeout][retransmission]"
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

    const auto outstanding =
        client.getOutstandingSegment(
            segment.seq
        );

    REQUIRE(
        outstanding.has_value()
    );

    REQUIRE(
        outstanding->seq ==
        segment.seq
    );

    REQUIRE(
        outstanding->payload ==
        segment.payload
    );
}

TEST_CASE(
    "TCP RTO only expires the oldest outstanding segment",
    "[tcp][rto][timeout]"
)
{
    Topology topology(2);

    topology.addLink(
        0,
        1,
        100.0,
        1.0,
        0.0,
        kns::LinkMode::FULL_DUPLEX
    );

    SimulationEngine engine(topology);

    auto& session =
        engine.createTCPSession(0, 1);

    auto& client =
        session.getClientConnection();

    client.send_syn();

    TCPSegment first;

    first.seq = client.getSendNext();
    first.payload.assign(
        100,
        0x41
    );
    first.flags =
        TCPFlag::ACK |
        TCPFlag::PSH;

    REQUIRE(
        client.queueSentSegment(
            first,
            engine.now()
        )
    );

    TCPSegment second;

    second.seq = client.getSendNext();
    second.payload.assign(
        100,
        0x42
    );
    second.flags =
        TCPFlag::ACK |
        TCPFlag::PSH;

    REQUIRE(
        client.queueSentSegment(
            second,
            engine.now()
        )
    );

    REQUIRE(
        client.getOldestOutstandingSequence()
            .value() == first.seq
    );

    const double initial_rto =
        client.getCurrentRTO();

    TCPTimeoutEvent second_timeout(
        engine.now(),
        session.getSession_id(),
        second.seq
    );

    REQUIRE_NOTHROW(
        second_timeout.execute(engine)
    );

    /*
     * The second segment is not the oldest one,
     * so its timeout must have no effect.
     */
    REQUIRE(
        client.getCurrentRTO() ==
        initial_rto
    );

    TCPTimeoutEvent first_timeout(
        engine.now(),
        session.getSession_id(),
        first.seq
    );

    REQUIRE_NOTHROW(
        first_timeout.execute(engine)
    );

    REQUIRE(
        client.getCurrentRTO() >
        initial_rto
    );
}