#include "network/tcp/TCPConnection.hpp"

#include <cstdlib>

namespace kns {

    TCPConnection::TCPConnection(TCPState state, 
                             int seq_num, 
                             int expected_ack_num, 
                             int local_node, 
                             int remote_node)
        : state(state),
        seq_num(seq_num),
        expected_ack_num(expected_ack_num),
        local_node(local_node),
        remote_node(remote_node) {}

    int64_t TCPConnection::send_syn() {
        seq_num = std::rand();
        state = TCPState::SYN_SENT;
        return seq_num;
    }

    void TCPConnection::receive_syn(uint32_t remote_seq) {
        expected_ack_num = remote_seq + 1;

        seq_num = std::rand();
        state = TCPState::SYN_RECEIVED;
    }

    int64_t TCPConnection::send_syn_ack() {
        return seq_num;
    }

    void TCPConnection::receive_syn_ack(uint32_t remote_seq, uint32_t remote_ack) {
        if (remote_ack == seq_num + 1) {
            expected_ack_num = remote_seq + 1;
            state = TCPState::ESTABLISHED;
        }
    }

    int64_t TCPConnection::send_ack() {
        return expected_ack_num;
    }

    void TCPConnection::receive_ack(uint32_t remote_ack) {
        if (state == TCPState::SYN_RECEIVED &&
            remote_ack == seq_num + 1) {
            state = TCPState::ESTABLISHED;
        }
    }
}