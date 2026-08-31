#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"

using kns::TCPConnection;
using kns::TCPState;

TEST_CASE("TCPConnection performs active close", "[tcp][close]")
{
    TCPConnection client(
        TCPState::ESTABLISHED,
        100,
        500,
        1,
        2
    );

    const bool fin_sent = client.send_fin();

    INFO("send_fin returned: " << std::boolalpha << fin_sent);
    INFO("state after send_fin: "
         << static_cast<int>(client.getTcpState()));
    INFO("sequence after send_fin: "
         << client.getSeqNum());
    INFO("expected ACK after send_fin: "
         << client.getExpectedAckNum());

    REQUIRE(fin_sent);
    REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_1);

    REQUIRE(client.receive_ack(101));
    REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_2);
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