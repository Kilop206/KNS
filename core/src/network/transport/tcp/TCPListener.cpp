#include "network/transport/tcp/TCPListener.hpp"

#include "engine/core/SimulationEngine.hpp"
#include "network/transport/tcp/TCPSession.hpp"

namespace kns {

    std::uint64_t TCPListener::accept(int source_node, SimulationEngine& engine)
    {
        if (!listening_) {
            return 0;
        }

        if (backlog_ > 0 && getActiveConnections() >= backlog_) {
            return 0;  // backlog full — caller may send RST
        }

        // Create a fresh session: server = node_id_, client = source_node.
        TCPSession& session = engine.createTCPSession(node_id_, source_node);
        const std::uint64_t sid = session.getSession_id();

        // Put the server-side connection in LISTEN so it can accept the SYN.
        // receive_syn() will transition it to SYN_RECEIVED.
        // (The client-side connection starts in CLOSED — it will transition
        //  to SYN_SENT when the client sends its next SYN, or we treat the
        //  incoming SYN as having already been sent.)

        trackSession(sid);

        if (on_accept_) {
            on_accept_(sid);
        }

        return sid;
    }

} // namespace kns
