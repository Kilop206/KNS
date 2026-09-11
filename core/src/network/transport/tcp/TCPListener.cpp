#include "network/transport/tcp/TCPListener.hpp"

#include "engine/core/SimulationEngine.hpp"
#include "network/transport/tcp/TCPSession.hpp"

namespace kns {

    std::uint64_t TCPListener::accept(
        int source_node,
        std::uint32_t source_seq,
        SimulationEngine& engine
    )
    {
        if (!listening_) {
            return INVALID_SESSION_ID;
        }

        if (isBacklogFull()) {
            return INVALID_SESSION_ID;
        }

        // Create a fresh session: server = node_id_, client = source_node.
        TCPSession& session =
            engine.createTCPSession(source_node, node_id_);

        const std::uint64_t sid =
            session.getSession_id();

        auto& client =
            session.getClientConnection();

        auto& server =
            session.getServerConnection();

        client.setSeqNum(source_seq);

        if (!client.markSynSent()) {
            return INVALID_SESSION_ID;
        }

        if (!server.onListen()) {
            return INVALID_SESSION_ID;
        }

        trackSession(sid);

        if (on_accept_) {
            on_accept_(sid);
        }

        return sid;
    }

} // namespace kns
