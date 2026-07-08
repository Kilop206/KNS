#include <catch2/catch_test_macros.hpp>

#include "network/transport/tcp/TCPStateMachine.hpp"

using kns::TCPState;
using kns::TCPStateMachine;

TEST_CASE("TCPStateMachine opens an active connection", "[tcp][state-machine]")
{
    TCPStateMachine machine;

    REQUIRE(machine.state() == TCPState::CLOSED);
    REQUIRE(machine.onSynSent());
    REQUIRE(machine.state() == TCPState::SYN_SENT);
    REQUIRE(machine.onEstablished());
    REQUIRE(machine.state() == TCPState::ESTABLISHED);
}

TEST_CASE("TCPStateMachine rejects invalid active open transition", "[tcp][state-machine]")
{
    TCPStateMachine machine(TCPState::ESTABLISHED);

    REQUIRE_FALSE(machine.onSynSent());
    REQUIRE(machine.state() == TCPState::ESTABLISHED);
}

TEST_CASE("TCPStateMachine follows active close path", "[tcp][state-machine]")
{
    TCPStateMachine machine(TCPState::ESTABLISHED);

    REQUIRE(machine.onFinSent());
    REQUIRE(machine.state() == TCPState::FIN_WAIT_1);
    REQUIRE(machine.onFinAcked());
    REQUIRE(machine.state() == TCPState::FIN_WAIT_2);
    REQUIRE(machine.onPeerFin());
    REQUIRE(machine.state() == TCPState::TIME_WAIT);
    REQUIRE(machine.onTimeWaitDone());
    REQUIRE(machine.state() == TCPState::CLOSED);
}

TEST_CASE("TCPStateMachine follows passive close path", "[tcp][state-machine]")
{
    TCPStateMachine machine(TCPState::ESTABLISHED);

    REQUIRE(machine.onPeerFin());
    REQUIRE(machine.state() == TCPState::CLOSE_WAIT);
    REQUIRE(machine.onFinSent());
    REQUIRE(machine.state() == TCPState::LAST_ACK);
    REQUIRE(machine.onFinAcked());
    REQUIRE(machine.state() == TCPState::CLOSED);
}
