#include <catch2/catch_test_macros.hpp>
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPTimeoutEvent.hpp"
#include "engine/events/TCPRetransmissionEvent.hpp"
#include "engine/events/TCPFastRetransmitEvent.hpp"
#include "network/transport/tcp/TCPListener.hpp"

using namespace kns;

TEST_CASE("Data retry exhaustion terminates both endpoints and releases the listener", "[tcp][loss]")
{
    Topology topology(2);
    topology.addLink(0, 1, 100.0, 1.0, 1.0);
    SimulationEngine engine(topology);
    auto& listener = engine.startTCPListen(1, 1);
    const auto id = engine.acceptOnListener(1, 0, 1000);
    REQUIRE(id != TCPListener::INVALID_SESSION_ID);
    auto& session = engine.getTCPSession(id);
    auto& client = session.getClientConnection();
    auto& server = session.getServerConnection();
    client = TCPConnection(TCPState::ESTABLISHED, 1000, 500, 0, 1);
    server = TCPConnection(TCPState::ESTABLISHED, 500, 1000, 1, 0);
    TCPSegment segment;
    segment.seq = client.getSendNext();
    segment.payload.assign(100, 0x41);
    segment.flags = TCPFlag::ACK | TCPFlag::PSH;
    REQUIRE(client.queueSentSegment(segment, engine.now()));
    for (std::uint32_t i = 0; i < TCPConnection::MAX_DATA_RETRANSMISSIONS; ++i) {
        REQUIRE(client.markSegmentRetransmitted(segment.seq, engine.now()));
    }
    SECTION("Timeout") { TCPTimeoutEvent(0, id, segment.seq).execute(engine); }
    SECTION("Retransmission") { TCPRetransmissionEvent(0, id, segment.seq).execute(engine); }
    SECTION("Fast retransmission") { TCPFastRetransmitEvent(0, id).execute(engine); }
    REQUIRE(client.getTcpState() == TCPState::CLOSED);
    REQUIRE(server.getTcpState() == TCPState::CLOSED);
    REQUIRE(client.getSendBufferSize() == 0);
    REQUIRE(listener.getActiveConnections() == 0);
    REQUIRE(session.getFailureReason() == TCPSession::FailureReason::DataRetriesExhausted);
    REQUIRE(engine.validateSimulation().failed_sessions == 1);
    REQUIRE(engine.validateSimulation().completed_sessions == 0);
    REQUIRE_FALSE(engine.validateSimulation().passed());
}
