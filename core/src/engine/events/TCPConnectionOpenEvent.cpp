#include "engine/events/TCPConnectionOpenEvent.hpp"

#include <iostream>

#include "engine/core/SimulationEngine.hpp"

namespace kns
{

    TCPConnectionOpenEvent::TCPConnectionOpenEvent(
            double timestamp,
            uint64_t session_id
        ) : Event(timestamp),
            session_id(session_id) {}

    bool sendPacketThroughTopology(
        SimulationEngine& engine,
        Packet& pkt
    )
    {
        int next =
            engine.getNextHop(
                pkt.current_node,
                pkt.destination
            );

        if (next == -1)
            return false;

        const auto& links =
            engine.getTopology().getLinksFromNode(
                pkt.current_node
            );

        for (const Link& link : links)
        {
            if (link.getOtherNode(pkt.current_node) == next)
            {
                engine.sendPacket(
                    pkt,
                    link,
                    engine.now()
                );

                return true;
            }
        }

        return false;
    }

    void TCPConnectionOpenEvent::execute(
        SimulationEngine& engine
    )
    {
        auto& session =
            engine.getTCPSession(
                session_id
            );

        auto& client =
            session.getClientConnection();

        auto& server =
            session.getServerConnection();

        if (
            server.getTcpState() == TCPState::SYN_RECEIVED &&
            client.getTcpState() != TCPState::ESTABLISHED
        )
        {
            server.send_syn_ack();

            Packet synAck(
                server.getLocalNode(),
                server.getRemoteNode(),
                server.getLocalNode(),
                engine.now(),
                engine.getGlobalPacketSize(),
                session_id
            );

            synAck.packet_type = PacketType::SYN_ACK;
            synAck.seq_num = server.getSeqNum();
            synAck.ack_num = server.getExpectedAckNum();

            sendPacketThroughTopology(
                engine,
                synAck
            );

            return;
        }

        if (
            client.getTcpState() == TCPState::ESTABLISHED &&
            server.getTcpState() == TCPState::SYN_RECEIVED
        )
        {
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

            sendPacketThroughTopology(
                engine,
                ack
            );

            return;
        }
    }
}