#include <catch2/catch_test_macros.hpp>
#include <limits>
#include "network/transport/tcp/TCPConnection.hpp"

using namespace kns;

TEST_CASE("TCP sends and acknowledges DATA across uint32 wrap", "[tcp][sequence]")
{
    constexpr auto start = std::numeric_limits<std::uint32_t>::max() - 49;
    TCPConnection sender(TCPState::ESTABLISHED, start, 10, 0, 1);
    TCPConnection receiver(TCPState::ESTABLISHED, 10, start, 1, 0);
    TCPSegment data;
    data.seq = start;
    data.payload.resize(100);
    REQUIRE(sender.queueSentSegment(data, 0.0));
    REQUIRE(sender.getSendNext() == 50);
    REQUIRE(sender.getSendNext() - sender.getSendUnacknowledged() == 100);
    REQUIRE_FALSE(sender.receive_ack(start - 1, 0.1));
    REQUIRE_FALSE(sender.receive_ack(51, 0.1));
    REQUIRE(receiver.receive_data(start, data.payload, 0.1));
    REQUIRE(receiver.getExpectedAckNum() == 50);
    REQUIRE(sender.receive_ack(50, 0.2));
    REQUIRE(sender.getSendUnacknowledged() == 50);
    REQUIRE(sender.getSendBufferSize() == 0);
}

TEST_CASE("Receive ordering overlap and duplicate checks cross uint32 wrap", "[tcp][sequence]")
{
    constexpr auto start = std::numeric_limits<std::uint32_t>::max() - 49;
    TCPReceiveBuffer buffer(start, 1000);
    auto entry = [](std::uint32_t seq, std::size_t size) {
        TCPSegment segment;
        segment.seq = seq;
        segment.payload.resize(size);
        return TCPReceiveEntry{segment, 0.0};
    };
    REQUIRE(buffer.push(entry(50, 100)));
    REQUIRE_FALSE(buffer.push(entry(75, 10)));
    REQUIRE_FALSE(buffer.push(entry(start - 1, 10)));
    REQUIRE_FALSE(buffer.push(entry(start + tcp_sequence::half_space, 10)));
    REQUIRE(buffer.bufferedBytes() == 100);
    REQUIRE(buffer.consumeContiguous() == 0);
    REQUIRE(buffer.push(entry(start, 100)));
    REQUIRE(buffer.consumeContiguous() == 2);
    REQUIRE(buffer.nextSequence() == 150);
    REQUIRE(buffer.bufferedBytes() == 0);
    REQUIRE_FALSE(buffer.push(entry(start, 100)));
    REQUIRE_THROWS_AS(buffer.setCapacity(tcp_sequence::half_space), std::invalid_argument);
    TCPConnection connection(TCPState::CLOSED, 0, 0, 0, 1);
    REQUIRE_THROWS_AS(connection.setSendWindow(tcp_sequence::half_space), std::invalid_argument);
}
