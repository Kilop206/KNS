#include "engine/events/TCPHanshakeTimeoutEvent.hpp"

#include "network/Packet.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

#include "engine/core/Log.hpp"

namespace kns {

    static constexpr double TCP_SYN_TIMEOUT = 1.0;

    TCPHandshakeTimeoutEvent::TCPHandshakeTimeoutEvent(
        double timestamp,
        std::uint64_t session_id
    )
        : Event(timestamp),
          session_id_(session_id)
    {
    }

    void TCPHandshakeTimeoutEvent::execute(SimulationEngine& engine)
    {
        auto& session = engine.getTCPSession(session_id_);
        auto& client = session.getClientConnection();

        if (client.getTcpState() != TCPState::SYN_SENT) {
            return;
        }

        if (!client.canRetrySyn()) {
            KNS_DEBUG_LOG(
                "[TCP][SESSION "
                << session_id_
                << "] SYN_RETRIES_EXHAUSTED\n");
            return;
        }

        client.incrementSynRetries();

        KNS_DEBUG_LOG(
            "[TCP][SESSION "
            << session_id_
            << "] SYN_TIMEOUT retry="
            << client.getSynRetries()
            << '\n');

        Packet syn(
            client.getLocalNode(),
            client.getRemoteNode(),
            client.getLocalNode(),
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        syn.tcp = client.buildSyn();
        syn.packet_type = inferPacketType(syn.tcp);
        syn.departure_time = engine.now();

        if (PacketUtils::sendPacketThroughTopology(engine, syn)) {
            engine.schedule(
                std::make_unique<TCPHandshakeTimeoutEvent>(
                    engine.now() + TCP_SYN_TIMEOUT,
                    session_id_
                )
            );
        }
    }

}