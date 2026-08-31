#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"

using kns::TCPConnection;
using kns::TCPState;

TEST_CASE("TCPConnection performs active close", "[tcp][close]")
{
    TCPConnection client(TCPState::ESTABLISHED, 100, 500, 1, 2);

    const auto fin = client.buildFin();

    REQUIRE(fin.fin());
    REQUIRE(fin.ackFlag());
    REQUIRE(fin.seq == 100);
    REQUIRE(fin.ack == 500);

    INFO("State: " << static_cast<int>(client.getTcpState()));
    INFO("Sequence: " << client.getSeqNum());
    INFO("Expected ACK: " << client.getExpectedAckNum());
    INFO("Receiving ACK: 101");
    
    REQUIRE(client.receive_ack(101));
    REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_2);

    REQUIRE(client.receive_fin(500));
    REQUIRE(client.getTcpState() == TCPState::TIME_WAIT);
    REQUIRE(client.getExpectedAckNum() == 501);
    REQUIRE(client.expire_time_wait());
    REQUIRE(client.getTcpState() == TCPState::CLOSED);
}

TEST_CASE("TCPConnection performs passive close", "[tcp][close]")
{
    TCPConnection server(TCPState::ESTABLISHED, 500, 101, 2, 1);

    REQUIRE(server.receive_fin(100));
    REQUIRE(server.getTcpState() == TCPState::CLOSE_WAIT);
    REQUIRE(server.getExpectedAckNum() == 101);

    REQUIRE(server.send_fin());
    REQUIRE(server.getTcpState() == TCPState::LAST_ACK);

    const auto fin = server.buildFin();
    REQUIRE(fin.seq == 500);
    REQUIRE(fin.ack == 101);

    REQUIRE(server.receive_ack(501));
    REQUIRE(server.getTcpState() == TCPState::CLOSED);
}

TEST_CASE("TCPConnection rejects FIN in closed state", "[tcp][close]")
{
    TCPConnection connection(TCPState::CLOSED, 100, 500, 1, 2);

    REQUIRE_FALSE(connection.receive_fin(500));
    REQUIRE(connection.getTcpState() == TCPState::CLOSED);
    REQUIRE(connection.getExpectedAckNum() == 500);
}
