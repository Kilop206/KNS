#include <catch2/catch_test_macros.hpp>
#include "network/transport/tcp/TCPConnection.hpp"

using kns::TCPConnection;
using kns::TCPState;

TEST_CASE("TCPConnection tracks SYN retransmission limits correctly", "[tcp][retransmission]")
{
    TCPConnection client(TCPState::CLOSED, 100, 0, 1, 2);

    REQUIRE(client.getSynRetries() == 0);
    REQUIRE(client.canRetrySyn());

    // Simulate retries
    for (std::uint32_t i = 1; i <= TCPConnection::MAX_SYN_RETRIES; ++i) {
        client.incrementSynRetries();
        REQUIRE(client.getSynRetries() == i);
        if (i < TCPConnection::MAX_SYN_RETRIES) {
            REQUIRE(client.canRetrySyn());
        } else {
            // Reached max retries
            REQUIRE_FALSE(client.canRetrySyn());
        }
    }

    // Reset should clear retries
    client.resetSynRetries();
    REQUIRE(client.getSynRetries() == 0);
    REQUIRE(client.canRetrySyn());
}
