#include "engine/events/TCPHanshakeTimeoutEvent.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/Packet.hpp"
#include "network/utils/PacketUtils.hpp"

#include <iostream>

namespace kns {

    static constexpr double TCP_SYN_TIMEOUT = 1.0;

    TCPHandshakeTimeoutEvent::TCPHandshakeTimeoutEvent(
        double timestamp,
        std::uint64_t session_id
    )
    : Event(timestamp),
      session_id(session_id)
    {
    }

    void TCPHandshakeTimeoutEvent::execute(SimulationEngine& engine)
    {
        auto& session = engine.getTCPSession(session_id);
        auto& client = session.getClientConnection();

        if (client.getTcpState() != TCPState::SYN_SENT) {
            return;
        }

        if (!client.canRetrySyn()) {
            std::cout << "[TCP][SESSION " << session_id
                      << "] SYN_RETRIES_EXHAUSTED\n";
            return;
        }

        client.incrementSynRetries();

        std::cout << "[TCP][SESSION " << session_id
                  << "] SYN_TIMEOUT retry="
                  << client.getSynRetries() << '\n';

        Packet syn(
            client.getLocalNode(),
            client.getRemoteNode(),
            client.getLocalNode(),
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id
        );
        syn.packet_type = PacketType::SYN;
        syn.seq_num = client.getSeqNum();
        syn.ack_num = 0;
        syn.departure_time = engine.now();

        if (PacketUtils::sendPacketThroughTopology(engine, syn)) {
            engine.schedule(std::make_unique<TCPHandshakeTimeoutEvent>(
                engine.now() + TCP_SYN_TIMEOUT,
                session_id
            ));
        }
    }

}