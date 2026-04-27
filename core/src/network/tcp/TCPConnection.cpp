#include "network/tcp/TCPConnection.hpp"

#include <cstdlib>

namespace kns {

    void TCPConnection::send_syn() {
        seq_num = std::rand();
        expected_ack_num = seq_num + 1;
        state = TCPState::SYN_SENT;
    }

    void TCPConnection::receive_syn(uint32_t remote_seq) {
        expected_ack_num = remote_seq + 1;

        seq_num = std::rand();
        expected_ack_num = seq_num + 1;

        state = TCPState::SYN_RECEIVED;
    }

    void TCPConnection::send_syn_ack() {

        state = TCPState::SYN_RECEIVED;
    }

    void TCPConnection::receive_syn_ack(uint32_t remote_seq, uint32_t remote_ack) {
        if (remote_ack == expected_ack_num) {
            expected_ack_num = remote_seq + 1;
            state = TCPState::ESTABLISHED;
        }
    }

    void TCPConnection::send_ack() {

        state = TCPState::ESTABLISHED;
    }

    void TCPConnection::receive_ack(uint32_t remote_ack) {
        if (state == TCPState::SYN_RECEIVED && remote_ack == expected_ack_num) {
            state = TCPState::ESTABLISHED;
        }
    }

}