#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPSegment.hpp"
#include "network/transport/tcp/congestion/CongestionControlType.hpp"

namespace {

kns::TCPSegment makeDataSegment(
    std::uint32_t sequence,
    std::size_t payload_size
)
{
    kns::TCPSegment segment;

    segment.seq = sequence;
    segment.ack = 0;
    segment.window = 65535;
    segment.flags = kns::TCPFlag::ACK | kns::TCPFlag::PSH;
    segment.payload.resize(payload_size);

    return segment;
}

kns::TCPConnection makeEstablishedConnection(
    std::uint32_t initial_sequence = 1000
)
{
    return kns::TCPConnection(
        kns::TCPState::ESTABLISHED,
        initial_sequence,
        2000,
        1,
        2,
        kns::CongestionControlType::RENO,
        1000,
        2000
    );
}

TEST_CASE(
    "TCPConnection records initial congestion sample",
    "[tcp][congestion][history]"
)
{
    auto connection =
        makeEstablishedConnection();

    const auto& history =
        connection.getCongestionHistory();

    REQUIRE_FALSE(history.empty());

    const auto& first =
        history.front();

    REQUIRE(first.timestamp == 0.0);
    REQUIRE(first.cwnd ==
        connection.getCongestionControl().getCwnd());
    REQUIRE(first.ssthresh ==
        connection.getCongestionControl().getSsthresh());
}

TEST_CASE(
    "TCPConnection records congestion change after ACK",
    "[tcp][congestion][history]"
)
{
    auto connection =
        makeEstablishedConnection();

    const auto initial_size =
        connection.getCongestionHistory().size();

    const auto initial_cwnd =
        connection.getCongestionControl().getCwnd();

    const auto segment =
        makeDataSegment(
            1000,
            1000
        );

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            2000,
            1.0
        )
    );

    const auto& history =
        connection.getCongestionHistory();

    REQUIRE(history.size() >
        initial_size);

    const auto& last =
        history.back();

    REQUIRE(last.timestamp == 1.0);
    REQUIRE(last.cwnd >
        initial_cwnd);
}

TEST_CASE(
    "TCPConnection does not duplicate identical congestion samples",
    "[tcp][congestion][history]"
)
{
    auto connection =
        makeEstablishedConnection();

    const auto before =
        connection.getCongestionHistory().size();

    connection.receive_ack(
        connection.getSendUnacknowledged(),
        1.0
    );

    const auto after =
        connection.getCongestionHistory().size();

    REQUIRE(after == before);
}

TEST_CASE(
    "TCPConnection records congestion change after fast retransmit",
    "[tcp][congestion][history]"
)
{
    auto connection =
        makeEstablishedConnection();

    const auto segment =
        makeDataSegment(
            1000,
            1000
        );

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    const auto before =
        connection.getCongestionHistory().size();

    connection.onFastRetransmit(
        1000,
        2.0
    );

    const auto& history =
        connection.getCongestionHistory();

    REQUIRE(history.size() >
        before);

    const auto& last =
        history.back();

    REQUIRE(last.timestamp == 2.0);
    REQUIRE(
        last.ssthresh ==
        connection.getCongestionControl().getSsthresh()
    );
}

TEST_CASE(
    "TCPConnection congestion history preserves chronological order",
    "[tcp][congestion][history]"
)
{
    auto connection =
        makeEstablishedConnection();

    const auto segment =
        makeDataSegment(
            1000,
            1000
        );

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            2000,
            1.0
        )
    );

    connection.onFastRetransmit(
        1000,
        2.0
    );

    const auto& history =
        connection.getCongestionHistory();

    REQUIRE(history.size() >= 2);

    for (
        std::size_t i = 1;
        i < history.size();
        ++i
    ) {
        REQUIRE(
            history[i - 1].timestamp <=
            history[i].timestamp
        );
    }
}

TEST_CASE(
    "TCPConnection records congestion change after timeout",
    "[tcp][congestion][history]"
)
{
    auto connection =
        makeEstablishedConnection();

    const auto segment =
        makeDataSegment(
            1000,
            1000
        );

    REQUIRE(
        connection.queueSentSegment(
            segment,
            0.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            2000,
            1.0
        )
    );

    const auto before_timeout =
        connection.getCongestionHistory().size();

    const auto cwnd_before =
        connection.getCongestionControl().getCwnd();

    connection.onSendTimeout(
        5.0
    );

    const auto& history =
        connection.getCongestionHistory();

    REQUIRE(
        history.size() >
        before_timeout
    );

    const auto& last =
        history.back();

    REQUIRE(
        last.timestamp == 5.0
    );

    REQUIRE(
        last.cwnd !=
        cwnd_before
    );

    REQUIRE(
        last.cwnd ==
        connection.getCongestionControl().getCwnd()
    );

    REQUIRE(
        last.ssthresh ==
        connection.getCongestionControl().getSsthresh()
    );
}

}