#include <iostream>

#include "engine/events/TCPConnectionCloseEvent.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns {

    TCPConnectionCloseEvent::TCPConnectionCloseEvent(
        double timestamp,
        std::uint64_t sessionId
    )
        : Event(timestamp),
          session_id_(sessionId)
    {
    }

    void TCPConnectionCloseEvent::execute(
        SimulationEngine& engine
    )
    {
        auto& session =
            engine.getTCPSession(session_id_);

        if (session.isCloseRequest()) {
            return;
        }
        session.setCloseRequest(true);

        auto& client =
            session.getClientConnection();

        Packet fin(
            client.getLocalNode(),
            client.getRemoteNode(),
            client.getLocalNode(),
            engine.now(),
            engine.getGlobalPacketSize(),
            session.getSession_id()
        );

        fin.tcp = client.buildFin();
        fin.packet_type = inferPacketType(fin.tcp);

        PacketUtils::sendPacketThroughTopology(engine, fin);
    }
}