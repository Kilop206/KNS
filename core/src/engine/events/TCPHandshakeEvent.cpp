#include "engine/events/TCPHandshakeEvent.hpp"

#include "engine/events/TCPHandshakeTimeoutEvent.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

#include "engine/core/Log.hpp"

namespace kns {

    TCPHandshakeEvent::TCPHandshakeEvent(
        double timestamp,
        int source,
        int destination,
        std::uint64_t session_id
    )
        : Event(timestamp),
          source_(source),
          destination_(destination),
          session_id_(session_id)
    {
    }

    void TCPHandshakeEvent::execute(SimulationEngine& engine)
    {
        KNS_DEBUG_LOG(
            "[TCP] Handshake session "
            << session_id_
            << '\n');

        auto& session = engine.getTCPSession(session_id_);
        auto& client = session.getClientConnection();

        client.send_syn();

        Packet syn(
            source_,
            destination_,
            source_,
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        syn.tcp = client.buildSyn();
        syn.packet_type = inferPacketType(syn.tcp);
        syn.departure_time = engine.now();

        if (PacketUtils::sendPacketThroughTopology(engine, syn))
        {
            engine.schedule(
                std::make_unique<TCPHandshakeTimeoutEvent>(
                    engine.now() + 1.0,
                    session_id_
                )
            );

            KNS_DEBUG_LOG(
                "[TCP][SESSION "
                << session_id_
                << "] SYN timeout scheduled at "
                << engine.now() + 1.0
                << '\n');
        }
    }
}