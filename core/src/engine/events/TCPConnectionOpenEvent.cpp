#include "engine/events/TCPConnectionOpenEvent.hpp"

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
        TCPSession session = engine.getTCPSession(session_id);
        TCPConnection server = session.getServerConnection();

        if (server.getTcpState() == TCPState::SYN_RECEIVED) {
            server.send_syn_ack();

            int local = server.getLocalNode();
            int destination = server.getRemoteNode();
            int current_node = server.getLocalNode();
            PacketType packet_type = PacketType::SYN_ACK;
            uint64_t seq_num = server.getSeqNum();
            uint64_t ack_num = server.getExpectedAckNum();
        }
    }

}