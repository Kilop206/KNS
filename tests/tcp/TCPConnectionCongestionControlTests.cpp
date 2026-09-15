#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/congestion/CubicCongestionControl.hpp"
#include "network/transport/tcp/congestion/NewRenoCongestionControl.hpp"
#include "network/transport/tcp/congestion/RenoCongestionControl.hpp"
#include "network/transport/tcp/congestion/TahoeCongestionControl.hpp"

using namespace kns;

TEST_CASE("New DATA respects congestion limits before and after timeout", "[tcp][congestion][window]")
{
    for (const auto type : {CongestionControlType::TAHOE, CongestionControlType::RENO,
            CongestionControlType::NEW_RENO, CongestionControlType::CUBIC}) {
        TCPConnection connection(TCPState::ESTABLISHED, 1000, 0, 0, 1, type, 100);
        connection.setSendWindow(10000);
        TCPSegment segment;
        segment.seq = connection.getSendNext();
        segment.payload.assign(100, 0x41);
        REQUIRE(connection.queueSentSegment(segment, 0.0));
        REQUIRE_FALSE(connection.canSend(1));
        REQUIRE(connection.receive_ack(1100, 0.1));
        REQUIRE(connection.canSend(100));
        segment.seq = connection.getSendNext();
        REQUIRE(connection.queueSentSegment(segment, 0.2));
        connection.onSendTimeout(1.0);
        const auto window = connection.getCongestionControl().getCwnd();
        const auto available = window > 100 ? window - 100 : 0;
        REQUIRE_FALSE(connection.canSend(available + 1));
    }
}

TEST_CASE(
    "TCPConnection creates Reno congestion control by default",
    "[tcp][congestion][connection]"
)
{
    TCPConnection connection(
        TCPState::CLOSED,
        0,
        0,
        1,
        2
    );

    REQUIRE(
        connection.getCongestionControlType() ==
        CongestionControlType::RENO
    );

    REQUIRE(
        dynamic_cast<RenoCongestionControl*>(
            &connection.getCongestionControl()
        ) != nullptr
    );
}

TEST_CASE(
    "TCPConnection can select Tahoe congestion control",
    "[tcp][congestion][connection]"
)
{
    TCPConnection connection(
        TCPState::CLOSED,
        0,
        0,
        1,
        2,
        CongestionControlType::TAHOE,
        1000
    );

    REQUIRE(
        connection.getCongestionControlType() ==
        CongestionControlType::TAHOE
    );

    REQUIRE(
        dynamic_cast<TahoeCongestionControl*>(
            &connection.getCongestionControl()
        ) != nullptr
    );

    REQUIRE(
        connection.getCongestionControl().getMss() == 1000
    );
}

TEST_CASE(
    "TCPConnection can select Reno congestion control",
    "[tcp][congestion][connection]"
)
{
    TCPConnection connection(
        TCPState::CLOSED,
        0,
        0,
        1,
        2,
        CongestionControlType::RENO,
        1000
    );

    REQUIRE(
        dynamic_cast<RenoCongestionControl*>(
            &connection.getCongestionControl()
        ) != nullptr
    );
}

TEST_CASE(
    "TCPConnection can select NewReno congestion control",
    "[tcp][congestion][connection]"
)
{
    TCPConnection connection(
        TCPState::CLOSED,
        0,
        0,
        1,
        2,
        CongestionControlType::NEW_RENO,
        1000
    );

    REQUIRE(
        connection.getCongestionControlType() ==
        CongestionControlType::NEW_RENO
    );

    REQUIRE(
        dynamic_cast<NewRenoCongestionControl*>(
            &connection.getCongestionControl()
        ) != nullptr
    );
}

TEST_CASE(
    "TCPConnection can select CUBIC congestion control",
    "[tcp][congestion][connection]"
)
{
    TCPConnection connection(
        TCPState::CLOSED,
        0,
        0,
        1,
        2,
        CongestionControlType::CUBIC,
        1000
    );

    REQUIRE(
        connection.getCongestionControlType() ==
        CongestionControlType::CUBIC
    );

    REQUIRE(
        dynamic_cast<CubicCongestionControl*>(
            &connection.getCongestionControl()
        ) != nullptr
    );
}

TEST_CASE(
    "TCPConnection exposes congestion window state",
    "[tcp][congestion][connection]"
)
{
    TCPConnection connection(
        TCPState::CLOSED,
        0,
        0,
        1,
        2,
        CongestionControlType::RENO,
        1000
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() == 1000
    );

    REQUIRE(
        connection.getCongestionControl().getMss() == 1000
    );
}

TEST_CASE(
    "TCPConnection timeout notifies congestion control",
    "[tcp][congestion][connection][timeout]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1,
        CongestionControlType::RENO,
        1000,
        4000
    );

    TCPSegment first;
    first.seq = 1000;
    first.payload.assign(
        1000,
        0x41
    );

    REQUIRE(
        connection.queueSentSegment(
            first,
            10.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            2000,
            11.0
        )
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() == 2000
    );
    TCPSegment second;
    second.seq = 2000;
    second.payload.assign(
        1000,
        0x42
    );

    REQUIRE(
        connection.queueSentSegment(
            second,
            12.0
        )
    );

    REQUIRE(
        connection.getSendBufferSize() == 1
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() == 2000
    );

    connection.onSendTimeout();

    REQUIRE(
        connection.getCongestionControl().getSsthresh() == 2000
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() == 1000
    );

    REQUIRE(
        connection.getCurrentRTO() == 6.0
    );
}

TEST_CASE(
    "TCPConnection does not notify congestion control for stale ACK",
    "[tcp][congestion][connection][ack]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        5000,
        6000,
        0,
        1,
        CongestionControlType::RENO,
        1000,
        4000
    );

    TCPSegment segment;
    segment.seq = 5000;
    segment.payload.assign(
        1000,
        0x41
    );

    REQUIRE(
        connection.queueSentSegment(
            segment,
            10.0
        )
    );

    REQUIRE(
        connection.receive_ack(
            6000,
            11.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() == 2000
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            5999,
            12.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 0
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() == 2000
    );
}

TEST_CASE(
    "TCPConnection duplicate ACK inflates congestion window during recovery",
    "[tcp][congestion][connection][ack]"
)
{
    TCPConnection connection(
        TCPState::ESTABLISHED,
        1000,
        2000,
        0,
        1,
        CongestionControlType::RENO,
        1000,
        4000
    );

    connection.getCongestionControl().onFastRetransmit(
        4000
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() == 5000
    );

    const std::uint32_t initial_cwnd =
        connection.getCongestionControl().getCwnd();

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            10.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 1
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() ==
        initial_cwnd + 1000
    );

    REQUIRE_FALSE(
        connection.receive_ack(
            1000,
            11.0
        )
    );

    REQUIRE(
        connection.getDuplicateAckCount() == 2
    );

    REQUIRE(
        connection.getCongestionControl().getCwnd() ==
        initial_cwnd + 2 * 1000
    );
}
