#include "network/transport/tcp/TCPConnection.hpp"

#include <cstdlib>
#include <iostream>

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
        if (state != TCPState::SYN_SENT) {
            seq_num = std::rand();
        }

        state = TCPState::SYN_SENT;
        return seq_num;
    }

    void TCPConnection::receive_syn(uint32_t remote_seq)
    {
        expected_ack_num = remote_seq + 1;
        state = TCPState::SYN_RECEIVED;
    }

    int64_t TCPConnection::send_syn_ack()
    {
        state = TCPState::SYN_RECEIVED;
        return seq_num;
    }

    void TCPConnection::receive_syn_ack(uint32_t remote_seq, uint32_t remote_ack) {
        if (remote_ack == seq_num + 1) {
            expected_ack_num = remote_seq + 1;
            state = TCPState::ESTABLISHED;
            resetSynRetries();
        }
    }

    int64_t TCPConnection::send_ack()
    {
        state = TCPState::ESTABLISHED;
        return expected_ack_num;
    }

    void TCPConnection::receive_ack(uint32_t remote_ack)
    {

        if (
            state == TCPState::SYN_RECEIVED &&
            remote_ack == seq_num + 1
        ) {
            state = TCPState::ESTABLISHED;
        }
    }

    TCPState TCPConnection::getTcpState() const {
        return state;
    }

    void TCPConnection::setTcpState(TCPState state) {
        this->state = state;
    }

    int TCPConnection::getLocalNode() const {
        return local_node;
    }

    int TCPConnection::getRemoteNode() const {
        return remote_node;
    }

    int TCPConnection::getSeqNum() const {
        return seq_num;
    }

    int TCPConnection::getExpectedAckNum() const {
        return expected_ack_num;
    }

    
    void TCPConnection::incrementSynRetries() {
        ++syn_retries;
    }

    bool TCPConnection::canRetrySyn() const {
        return syn_retries < MAX_SYN_RETRIES;
    }

    int TCPConnection::getSynRetries() const {
        return syn_retries;
    }

    void TCPConnection::resetSynRetries() {
        syn_retries = 0;
    }
}