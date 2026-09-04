#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPSession.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "enums/TCPState.hpp"

using kns::TCPSession;
using kns::TCPConnection;
using kns::TCPState;

TEST_CASE("TCPSession aggregate state tracks 3-way handshake lifecycle", "[tcp][session][state]")
{
    TCPSession session(1, 10, 20, TCPState::CLOSED);

    auto& client = session.getClientConnection();
    auto& server = session.getServerConnection();

    // 1. Initial state
    REQUIRE(client.getTcpState() == TCPState::CLOSED);
    REQUIRE(server.getTcpState() == TCPState::CLOSED);
    REQUIRE(session.getState() == TCPState::CLOSED);

    // 2. Client sends SYN -> SYN_SENT
    REQUIRE(client.send_syn());
    REQUIRE(client.getTcpState() == TCPState::SYN_SENT);
    REQUIRE(server.getTcpState() == TCPState::CLOSED);
    REQUIRE(session.getState() == TCPState::SYN_SENT);

    // 3. Server receives SYN -> SYN_RECEIVED
    REQUIRE(server.receive_syn(client.getSeqNum()));
    REQUIRE(server.getTcpState() == TCPState::SYN_RECEIVED);
    REQUIRE(client.getTcpState() == TCPState::SYN_SENT);
    REQUIRE(session.getState() == TCPState::SYN_RECEIVED);

    // 4. Client receives SYN-ACK -> ESTABLISHED (server still in SYN_RECEIVED)
    REQUIRE(server.send_syn_ack());
    REQUIRE(client.receive_syn_ack(server.getSeqNum(), client.getSeqNum() + 1));
    REQUIRE(client.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(server.getTcpState() == TCPState::SYN_RECEIVED);
    REQUIRE(session.getState() == TCPState::SYN_RECEIVED);

    // 5. Server receives ACK -> ESTABLISHED (both ESTABLISHED)
    REQUIRE(server.receive_ack(server.getSeqNum() + 1));
    REQUIRE(client.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(server.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(session.getState() == TCPState::ESTABLISHED);
}

TEST_CASE("TCPSession aggregate state tracks active close lifecycle", "[tcp][session][state]")
{
    TCPSession session(1, 10, 20, TCPState::CLOSED);

    // Establish connections
    session.getClientConnection() = TCPConnection(TCPState::ESTABLISHED, 100, 500, 10, 20);
    session.getServerConnection() = TCPConnection(TCPState::ESTABLISHED, 500, 100, 20, 10);

    auto& client = session.getClientConnection();
    auto& server = session.getServerConnection();

    REQUIRE(session.getState() == TCPState::ESTABLISHED);

    // 1. Client initiates close (FIN_WAIT_1)
    REQUIRE(client.send_fin());
    REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_1);
    REQUIRE(server.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(session.getState() == TCPState::FIN_WAIT_1);

    // 2. Server receives FIN -> CLOSE_WAIT
    REQUIRE(server.receive_fin(client.getSeqNum()));
    REQUIRE(server.getTcpState() == TCPState::CLOSE_WAIT);
    REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_1);
    REQUIRE(session.getState() == TCPState::CLOSE_WAIT);

    // 3. Client receives ACK -> FIN_WAIT_2
    REQUIRE(client.receive_ack(client.getSeqNum() + 1));
    REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_2);
    REQUIRE(server.getTcpState() == TCPState::CLOSE_WAIT);
    REQUIRE(session.getState() == TCPState::CLOSE_WAIT);

    // 4. Server sends FIN -> LAST_ACK
    REQUIRE(server.send_fin());
    REQUIRE(server.getTcpState() == TCPState::LAST_ACK);
    REQUIRE(client.getTcpState() == TCPState::FIN_WAIT_2);
    REQUIRE(session.getState() == TCPState::LAST_ACK);

    // 5. Client receives server's FIN -> TIME_WAIT
    REQUIRE(client.receive_fin(server.getSeqNum()));
    REQUIRE(client.getTcpState() == TCPState::TIME_WAIT);
    REQUIRE(server.getTcpState() == TCPState::LAST_ACK);
    REQUIRE(session.getState() == TCPState::LAST_ACK);

    // 6. Server receives ACK -> CLOSED
    REQUIRE(server.receive_ack(server.getSeqNum() + 1));
    REQUIRE(server.getTcpState() == TCPState::CLOSED);
    REQUIRE(client.getTcpState() == TCPState::TIME_WAIT);
    REQUIRE(session.getState() == TCPState::TIME_WAIT);

    // 7. TIME_WAIT expires -> CLOSED
    REQUIRE(client.expire_time_wait());
    REQUIRE(client.getTcpState() == TCPState::CLOSED);
    REQUIRE(server.getTcpState() == TCPState::CLOSED);
    REQUIRE(session.getState() == TCPState::CLOSED);
}

TEST_CASE("TCPSession aggregate state tracks passive close lifecycle", "[tcp][session][state]")
{
    TCPSession session(1, 10, 20, TCPState::CLOSED);

    // Server initiates close
    session.getClientConnection() = TCPConnection(TCPState::ESTABLISHED, 100, 500, 10, 20);
    session.getServerConnection() = TCPConnection(TCPState::ESTABLISHED, 500, 100, 20, 10);

    auto& client = session.getClientConnection();
    auto& server = session.getServerConnection();

    // 1. Server sends FIN -> FIN_WAIT_1
    REQUIRE(server.send_fin());
    REQUIRE(server.getTcpState() == TCPState::FIN_WAIT_1);
    REQUIRE(client.getTcpState() == TCPState::ESTABLISHED);
    REQUIRE(session.getState() == TCPState::FIN_WAIT_1);

    // 2. Client receives FIN -> CLOSE_WAIT
    REQUIRE(client.receive_fin(server.getSeqNum()));
    REQUIRE(client.getTcpState() == TCPState::CLOSE_WAIT);
    REQUIRE(session.getState() == TCPState::CLOSE_WAIT);

    // 3. Server receives ACK -> FIN_WAIT_2
    REQUIRE(server.receive_ack(server.getSeqNum() + 1));
    REQUIRE(server.getTcpState() == TCPState::FIN_WAIT_2);
    REQUIRE(client.getTcpState() == TCPState::CLOSE_WAIT);
    REQUIRE(session.getState() == TCPState::CLOSE_WAIT);

    // 4. Client sends FIN -> LAST_ACK
    REQUIRE(client.send_fin());
    REQUIRE(client.getTcpState() == TCPState::LAST_ACK);
    REQUIRE(session.getState() == TCPState::LAST_ACK);

    // 5. Server receives FIN -> TIME_WAIT
    REQUIRE(server.receive_fin(client.getSeqNum()));
    REQUIRE(server.getTcpState() == TCPState::TIME_WAIT);
    REQUIRE(client.getTcpState() == TCPState::LAST_ACK);
    REQUIRE(session.getState() == TCPState::LAST_ACK);

    // 6. Client receives ACK -> CLOSED
    REQUIRE(client.receive_ack(client.getSeqNum() + 1));
    REQUIRE(client.getTcpState() == TCPState::CLOSED);
    REQUIRE(server.getTcpState() == TCPState::TIME_WAIT);
    REQUIRE(session.getState() == TCPState::TIME_WAIT);

    // 7. TIME_WAIT expires on server -> CLOSED
    REQUIRE(server.expire_time_wait());
    REQUIRE(server.getTcpState() == TCPState::CLOSED);
    REQUIRE(client.getTcpState() == TCPState::CLOSED);
    REQUIRE(session.getState() == TCPState::CLOSED);
}

TEST_CASE("TCPSession aggregate state evaluates required state matrix", "[tcp][session][state]")
{
    TCPSession session(1, 10, 20, TCPState::CLOSED);

    auto test_matrix = [&](TCPState client_st, TCPState server_st, TCPState expected_session_st) {
        session.getClientConnection() = TCPConnection(client_st, 0, 0, 10, 20);
        session.getServerConnection() = TCPConnection(server_st, 0, 0, 20, 10);
        REQUIRE(session.getState() == expected_session_st);
    };

    SECTION("Required combinations from issue #99 specification")
    {
        test_matrix(TCPState::CLOSED, TCPState::CLOSED, TCPState::CLOSED);
        test_matrix(TCPState::CLOSED, TCPState::SYN_RECEIVED, TCPState::SYN_RECEIVED);
        test_matrix(TCPState::SYN_SENT, TCPState::SYN_RECEIVED, TCPState::SYN_RECEIVED);
        test_matrix(TCPState::ESTABLISHED, TCPState::ESTABLISHED, TCPState::ESTABLISHED);
        test_matrix(TCPState::ESTABLISHED, TCPState::CLOSE_WAIT, TCPState::CLOSE_WAIT);
        test_matrix(TCPState::FIN_WAIT_1, TCPState::ESTABLISHED, TCPState::FIN_WAIT_1);
        test_matrix(TCPState::FIN_WAIT_2, TCPState::CLOSE_WAIT, TCPState::CLOSE_WAIT);
        test_matrix(TCPState::TIME_WAIT, TCPState::CLOSED, TCPState::TIME_WAIT);
        test_matrix(TCPState::CLOSED, TCPState::LAST_ACK, TCPState::LAST_ACK);
    }

    SECTION("Symmetrical combinations")
    {
        test_matrix(TCPState::SYN_RECEIVED, TCPState::CLOSED, TCPState::SYN_RECEIVED);
        test_matrix(TCPState::CLOSE_WAIT, TCPState::ESTABLISHED, TCPState::CLOSE_WAIT);
        test_matrix(TCPState::ESTABLISHED, TCPState::FIN_WAIT_1, TCPState::FIN_WAIT_1);
        test_matrix(TCPState::CLOSE_WAIT, TCPState::FIN_WAIT_2, TCPState::CLOSE_WAIT);
        test_matrix(TCPState::CLOSED, TCPState::TIME_WAIT, TCPState::TIME_WAIT);
        test_matrix(TCPState::LAST_ACK, TCPState::CLOSED, TCPState::LAST_ACK);
    }

    SECTION("Simultaneous close (CLOSING)")
    {
        test_matrix(TCPState::CLOSING, TCPState::CLOSING, TCPState::CLOSING);
        test_matrix(TCPState::CLOSING, TCPState::TIME_WAIT, TCPState::CLOSING);
    }

    SECTION("Listen states")
    {
        test_matrix(TCPState::LISTEN, TCPState::LISTEN, TCPState::LISTEN);
        test_matrix(TCPState::SYN_SENT, TCPState::LISTEN, TCPState::SYN_SENT);
    }
}

TEST_CASE("TCPSession aggregate state updates immediately without stale caching", "[tcp][session][state]")
{
    TCPSession session(1, 10, 20, TCPState::CLOSED);

    REQUIRE(session.getState() == TCPState::CLOSED);

    // Change client connection state directly
    session.getClientConnection() = TCPConnection(TCPState::SYN_SENT, 0, 0, 10, 20);
    REQUIRE(session.getState() == TCPState::SYN_SENT);

    // Change server connection state directly
    session.getServerConnection() = TCPConnection(TCPState::SYN_RECEIVED, 0, 0, 20, 10);
    REQUIRE(session.getState() == TCPState::SYN_RECEIVED);

    // Both established
    session.getClientConnection() = TCPConnection(TCPState::ESTABLISHED, 0, 0, 10, 20);
    session.getServerConnection() = TCPConnection(TCPState::ESTABLISHED, 0, 0, 20, 10);
    REQUIRE(session.getState() == TCPState::ESTABLISHED);
}
