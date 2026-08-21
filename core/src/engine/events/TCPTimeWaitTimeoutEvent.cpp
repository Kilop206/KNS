#include "engine/events/TCPTimeWaitTimeoutEvent.hpp"

#include "engine/core/SimulationEngine.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "enums/TCPState.hpp"

namespace kns {
    TCPTimeWaitTimeoutEvent::TCPTimeWaitTimeoutEvent(double timestamp, std::uint64_t session_id)
        : Event(timestamp),
          session_id_(session_id)
    {
    }

    void TCPTimeWaitTimeoutEvent::execute(SimulationEngine& engine) {
        auto& session = engine.getTCPSession(session_id_);
        auto& client = session.getClientConnection();

        if (client.getTcpState() == TCPState::TIME_WAIT) {
            client.setTcpState(TCPState::CLOSED);
        }

        session.setState(TCPState::CLOSED);
    }
}