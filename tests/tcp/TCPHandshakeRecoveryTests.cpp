#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"

using namespace kns;

TEST_CASE("SYN loss preserves bounded handshake retries", "[tcp][handshake][loss]")
{
    Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 100.0, 1.0, 1.0);
    SimulationEngine engine(topology);
    engine.startTCPConnection(0, 1);
    const auto id = engine.getTCPSessions().begin()->first;
    REQUIRE(engine.processEvent());
    REQUIRE(engine.hasEvents());
    REQUIRE(engine.processEvent());
    REQUIRE(engine.getTCPSession(id).getClientConnection().getSynRetries() == 1);
    REQUIRE(engine.hasEvents());

    SECTION("Connectivity returns before retries are exhausted") {
        link->setLossProb(0.0);
        for (int i = 0; i < 10 &&
                engine.getTCPSession(id).getClientConnection().getTcpState() == TCPState::SYN_SENT; ++i) {
            REQUIRE(engine.processEvent());
        }
        REQUIRE(engine.getTCPSession(id).getClientConnection().getTcpState() == TCPState::ESTABLISHED);
    }
    SECTION("Permanent loss has a finite retry budget") {
        int events = 0;
        while (engine.hasEvents() && events < 100) {
            engine.processEvent();
            ++events;
        }
        REQUIRE(events < 100);
        REQUIRE_FALSE(engine.hasEvents());
        REQUIRE_FALSE(engine.getTCPSession(id).getClientConnection().canRetrySyn());
    }
}
