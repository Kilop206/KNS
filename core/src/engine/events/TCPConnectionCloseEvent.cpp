#include "engine/core/Log.hpp"
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

    void TCPConnectionCloseEvent::execute(SimulationEngine& engine)
    {
        auto& session = engine.getTCPSession(session_id_);

        auto& client = session.getClientConnection();

        if (client.getTcpState() != TCPState::ESTABLISHED) {
            return;
        }

        KNS_DEBUG_LOG(
            "[TCP] Closing session "
            << session_id_
            << '\n');

        if (!client.send_fin()) {
            KNS_DEBUG_LOG(
                "[TCP] send_fin() rejected in state "
                << static_cast<int>(client.getTcpState())
                << " — close aborted\n");
            return;
        }

        Packet fin(
            client.getLocalNode(),
            client.getRemoteNode(),
            client.getLocalNode(),
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        fin.tcp = client.buildFin();
        fin.packet_type = inferPacketType(fin.tcp);
        fin.departure_time = engine.now();

        if (PacketUtils::sendPacketThroughTopology(engine, fin)) {
            KNS_DEBUG_LOG(
                "[TCP] FIN sent for session "
                << session_id_
                << '\n');
        }
    }
}