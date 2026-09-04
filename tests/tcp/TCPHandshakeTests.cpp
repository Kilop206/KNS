#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPConnection.hpp"

using kns::TCPConnection;
using kns::TCPState;

TEST_CASE("TCPConnection completes three way handshake state changes", "[tcp][handshake]")
{
    TCPConnection client(TCPState::CLOSED, 100, 0, 1, 2);
    TCPConnection server(TCPState::LISTEN, 500, 0, 2, 1);

    REQUIRE(client.send_syn());
    REQUIRE(client.getTcpState() == TCPState::SYN_SENT);

    const auto clientSeq = client.getSeqNum();

    REQUIRE(server.receive_syn(clientSeq));
    REQUIRE(server.getTcpState() == TCPState::SYN_RECEIVED);
    REQUIRE(server.getExpectedAckNum() == clientSeq + 1);

    const auto synAck = server.buildSynAck();
    REQUIRE(synAck.syn());
    REQUIRE(synAck.ackFlag());
    REQUIRE(synAck.seq == 500);
    REQUIRE(synAck.ack == clientSeq + 1);

    REQUIRE(client.receive_syn_ack(synAck.seq, synAck.ack));
    REQUIRE(client.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(client.getExpectedAckNum() == 501);

    const auto ack = client.buildAck();
    REQUIRE(ack.ackFlag());
    REQUIRE_FALSE(ack.syn());
    REQUIRE(ack.ack == 501);

    REQUIRE(server.receive_ack(ack.ack, 1.0));
    REQUIRE(server.getTcpState() == TCPState::ESTABLISHED);
}

TEST_CASE("TCPConnection ignores SYN ACK with unexpected ack number", "[tcp][handshake]")
{
    TCPConnection client(TCPState::SYN_SENT, 100, 0, 1, 2);

    REQUIRE_FALSE(client.receive_syn_ack(500, 999));
    REQUIRE(client.getTcpState() == TCPState::SYN_SENT);
}

TEST_CASE("TCPConnection rejects invalid SYN after establishment", "[tcp][handshake]")
{
    TCPConnection connection(TCPState::ESTABLISHED, 100, 0, 1, 2);

    REQUIRE_FALSE(connection.receive_syn(500));
    REQUIRE(connection.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(connection.getExpectedAckNum() == 0);
}
