#include "engine/events/TCPHandshakeEvent.hpp"

namespace kns {
    void TCPHandshakeEvent::execute(TCPConnection conn) {
        int seq = conn.send_syn();
        conn.receive_syn(seq + 1);
        int ack = conn.send_syn_ack();
        conn.receive_syn_ack(seq, ack);
        int remote_ack = conn.send_ack();
        conn.receive_ack(remote_ack);
    }
}