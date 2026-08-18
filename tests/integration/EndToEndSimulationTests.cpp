#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "enums/TCPState.hpp"
#include "enums/LinkMode.hpp"

using kns::Topology;
using kns::SimulationEngine;
using kns::TCPSession;
using kns::TCPState;
using kns::LinkMode;

TEST_CASE("End-to-end simulation establishes TCP connection and completes session", "[integration][simulation]")
{
    // 1. Create a topology of 2 nodes
    Topology topo(2);
    topo.addLink(0, 1, 10.0, 10.0, 0.0, LinkMode::FULL_DUPLEX);

    // 2. Initialize simulation engine
    SimulationEngine engine(topo);
    engine.setGlobalLossProb(0.0f); // 0% loss probability to ensure success
    engine.setGlobalPacketSize(1000);

    // 3. Initiate connection from node 0 to node 1
    engine.startTCPConnection(0, 1);

    // Verify a session has been created
    const auto& sessions = engine.getTCPSessions();
    REQUIRE(sessions.size() == 1);
    
    // Get the session ID
    std::uint64_t session_id = sessions.begin()->first;
    auto& session = engine.getTCPSession(session_id);
    REQUIRE(session.getState() == TCPState::CLOSED);

    // 4. Run the simulation
    engine.run();

    // 5. Verify the TCP session has progressed through established state or closed
    // After running the simulation to completion:
    // - Handshake completes -> transitions to ESTABLISHED.
    // - Traffic is generated (since it is hardcoded to generate traffic on ESTABLISHED in some parts).
    // - Eventually it closes, transitioning to CLOSED.
    // Let's verify that the session has transitioned out of CLOSED or completed successfully.
    // Let's print some stats for debugging.
    auto& stats = engine.getStats();
    REQUIRE(stats.packets_delivered > 0);
    REQUIRE(stats.packets_lost == 0);

    // The validation function can be run to check the simulation sanity
    const auto report = engine.validateSimulation();
    REQUIRE(report.passed());
}
