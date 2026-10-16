#include "engine/events/TCPHandshakeTimeoutEvent.hpp"

#include "engine/core/SimulationEngine.hpp"
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
        if (!engine.hasTCPSession(session_id_)) {
            return;
        }

        auto& session = engine.getTCPSession(session_id_);
        auto& client = session.getClientConnection();
        auto& server = session.getServerConnection();
        const bool awaiting_final_ack = client.getTcpState() == TCPState::ESTABLISHED &&
            server.getTcpState() == TCPState::SYN_RECEIVED;

        if (client.getTcpState() != TCPState::SYN_SENT && !awaiting_final_ack) {
            return;
        }

        auto& sender = awaiting_final_ack ? server : client;
        if (!sender.canRetrySyn()) {
            if (session.failHandshake()) {
                engine.releaseTCPListenerSession(session_id_);
            }
            KNS_DEBUG_LOG(
                "[TCP][SESSION "
                << session_id_
                << "] SYN_RETRIES_EXHAUSTED\n");
            return;
        }

        sender.incrementSynRetries();

        KNS_DEBUG_LOG(
            "[TCP][SESSION "
            << session_id_
            << "] SYN_TIMEOUT retry="
            << sender.getSynRetries()
            << '\n');

        Packet syn(
            sender.getLocalNode(),
            sender.getRemoteNode(),
            sender.getLocalNode(),
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        syn.tcp = awaiting_final_ack ? sender.buildSynAck() : sender.buildSyn();
        syn.packet_type = inferPacketType(syn.tcp);
        syn.departure_time = engine.now();

        PacketUtils::sendPacketThroughTopology(engine, syn);
        {
            engine.schedule(
                std::make_unique<TCPHandshakeTimeoutEvent>(
                    engine.now() + TCP_SYN_TIMEOUT,
                    session_id_
                )
            );
        }
    }

}
