#include "engine/events/TCPConnectionOpenEvent.hpp"
#include "enums/PacketType.hpp"
#include "network/utils/PacketUtils.hpp"

#include <iostream>

namespace kns {

    TCPConnectionOpenEvent::TCPConnectionOpenEvent(
        double timestamp,
        std::uint64_t session_id
    )
    : Event(timestamp),
      session_id(session_id)
    {
    }

    void TCPConnectionOpenEvent::execute(
        SimulationEngine& engine
    )
    {
        std::cout << "[TCP][SESSION "
            << session_id
            << "] TCPConnectionOpenEvent\n";

        TCPSession& session = engine.getTCPSession(session_id);
        TCPConnection& server = session.getServerConnection();
        TCPConnection& client = session.getClientConnection();

        if (server.getTcpState() == TCPState::SYN_RECEIVED
            && client.getTcpState() != TCPState::ESTABLISHED) {
            server.send_syn_ack();

            Packet syn_ack(
                server.getLocalNode(),
                server.getRemoteNode(),
                server.getLocalNode(),
                engine.now(),
                engine.getGlobalPacketSize(),
                session_id
            );

            syn_ack.packet_type = PacketType::SYN_ACK;
            syn_ack.seq_num = server.getSeqNum();
            syn_ack.ack_num = server.getExpectedAckNum();
            syn_ack.departure_time = engine.now();

            sendPacketThroughTopology(engine, syn_ack);
        } else if (server.getTcpState() == TCPState::SYN_RECEIVED 
                    && client.getTcpState() == TCPState::ESTABLISHED) 
        {

            std::cout
                << "[TCP][SESSION "
                << session_id
                << "] SENDING_FINAL_ACK\n";

            Packet ack(
                client.getLocalNode(),
                client.getRemoteNode(),
                client.getLocalNode(),
                engine.now(),
                engine.getGlobalPacketSize(),
                session_id
            );

            ack.packet_type = PacketType::ACK;
            ack.seq_num = client.getSeqNum();
            ack.ack_num = client.send_ack();
            ack.departure_time = engine.now();

            sendPacketThroughTopology(engine, ack);
        }
    }
}