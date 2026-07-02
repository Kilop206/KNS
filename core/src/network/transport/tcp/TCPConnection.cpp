#include "network/transport/tcp/TCPConnection.hpp"

#include <cstdlib>
#include <iostream>

namespace kns {

    std::uint32_t TCPConnection::generateInitialSeq()
    {
        return static_cast<std::uint32_t>(std::rand());
    }

    TCPConnection::TCPConnection(
        TCPState state,
        std::uint32_t seq_num,
        std::uint32_t expected_ack_num,
        int local_node,
        int remote_node
    )
        : state_machine_(state),
          seq_num_(seq_num),
          expected_ack_num_(expected_ack_num),
          local_node_(local_node),
          remote_node_(remote_node)
    {
    }

    TCPState TCPConnection::getTcpState() const noexcept
    {
        return state_machine_.state();
    }

    void TCPConnection::setTcpState(TCPState state) noexcept
    {
        state_machine_.setState(state);
    }

    int TCPConnection::getLocalNode() const noexcept
    {
        return local_node_;
    }

    int TCPConnection::getRemoteNode() const noexcept
    {
        return remote_node_;
    }

    std::uint32_t TCPConnection::getSeqNum() const noexcept
    {
        return seq_num_;
    }

    void TCPConnection::setSeqNum(std::uint32_t value) noexcept
    {
        seq_num_ = value;
    }

    std::uint32_t TCPConnection::getExpectedAckNum() const noexcept
    {
        return expected_ack_num_;
    }

    void TCPConnection::setExpectedAckNum(std::uint32_t value) noexcept
    {
        expected_ack_num_ = value;
    }

    std::uint32_t TCPConnection::getSynRetries() const noexcept
    {
        return syn_retries_;
    }

    void TCPConnection::incrementSynRetries() noexcept
    {
        ++syn_retries_;
    }

    bool TCPConnection::canRetrySyn() const noexcept
    {
        return syn_retries_ < MAX_SYN_RETRIES;
    }

    void TCPConnection::resetSynRetries() noexcept
    {
        syn_retries_ = 0;
    }

    bool TCPConnection::isEstablished() const noexcept
    {
        return getTcpState() == TCPState::ESTABLISHED;
    }

    bool TCPConnection::isClosed() const noexcept
    {
        return getTcpState() == TCPState::CLOSED;
    }

    TCPSegment TCPConnection::buildSyn() const
    {
        TCPSegment seg;
        seg.seq = seq_num_;
        seg.ack = 0;
        seg.window = 0;
        seg.flags = TCPFlag::SYN;
        return seg;
    }

    TCPSegment TCPConnection::buildSynAck() const
    {
        TCPSegment seg;
        seg.seq = seq_num_;
        seg.ack = expected_ack_num_;
        seg.window = 0;
        seg.flags = TCPFlag::SYN | TCPFlag::ACK;
        return seg;
    }

    TCPSegment TCPConnection::buildAck() const
    {
        TCPSegment seg;
        seg.seq = seq_num_;
        seg.ack = expected_ack_num_;
        seg.window = 0;
        seg.flags = TCPFlag::ACK;
        return seg;
    }

    TCPSegment TCPConnection::buildFin() const
    {
        TCPSegment seg;
        seg.seq = seq_num_;
        seg.ack = expected_ack_num_;
        seg.window = 0;
        seg.flags = TCPFlag::FIN | TCPFlag::ACK;
        return seg;
    }

    void TCPConnection::receive_syn(std::uint32_t remote_seq)
    {
        expected_ack_num_ = remote_seq + 1;
        state_machine_.setState(TCPState::SYN_RECEIVED);
    }

    bool TCPConnection::receive_syn_ack(std::uint32_t remote_seq, std::uint32_t remote_ack)
    {
        if (getTcpState() != TCPState::SYN_SENT) {
            return false;
        }

        if (remote_ack != seq_num_ + 1) {
            return false;
        }

        expected_ack_num_ = remote_seq + 1;
        state_machine_.setState(TCPState::ESTABLISHED);
        resetSynRetries();
        return true;
    }

    bool TCPConnection::receive_ack(std::uint32_t remote_ack)
    {
        std::cout
            << "[DEBUG ACK] state="
            << static_cast<int>(getTcpState())
            << " remote_ack="
            << remote_ack
            << " expected="
            << (seq_num_ + 1)
            << '\n';

        if (getTcpState() == TCPState::SYN_RECEIVED &&
            remote_ack == seq_num_ + 1) {
            std::cout << "[DEBUG ACK] SERVER ESTABLISHED\n";
            state_machine_.setState(TCPState::ESTABLISHED);
            resetSynRetries();
            return true;
        }

        if (getTcpState() == TCPState::FIN_WAIT_1 &&
            remote_ack == seq_num_ + 1) {
            state_machine_.setState(TCPState::FIN_WAIT_2);
            return true;
        }

        if (getTcpState() == TCPState::LAST_ACK &&
            remote_ack == seq_num_ + 1) {
            state_machine_.setState(TCPState::CLOSED);
            return true;
        }

        return false;
    }

    void TCPConnection::receive_fin(std::uint32_t remote_seq)
    {
        expected_ack_num_ = remote_seq + 1;

        if (getTcpState() == TCPState::ESTABLISHED) {
            state_machine_.setState(TCPState::CLOSE_WAIT);
        } else if (getTcpState() == TCPState::FIN_WAIT_1) {
            state_machine_.setState(TCPState::CLOSING);
        } else if (getTcpState() == TCPState::FIN_WAIT_2) {
            state_machine_.setState(TCPState::TIME_WAIT);
        }
    }

    std::uint32_t TCPConnection::send_syn()
    {
        if (getTcpState() != TCPState::SYN_SENT) {
            seq_num_ = generateInitialSeq();
        }

        state_machine_.setState(TCPState::SYN_SENT);
        return seq_num_;
    }

    std::uint32_t TCPConnection::send_syn_ack()
    {
        state_machine_.setState(TCPState::SYN_RECEIVED);
        return seq_num_;
    }

    std::uint32_t TCPConnection::send_ack()
    {
        state_machine_.setState(TCPState::ESTABLISHED);
        return expected_ack_num_;
    }

    std::uint32_t TCPConnection::send_fin()
    {
        if (getTcpState() == TCPState::ESTABLISHED) {
            state_machine_.setState(TCPState::FIN_WAIT_1);
        } else if (getTcpState() == TCPState::CLOSE_WAIT) {
            state_machine_.setState(TCPState::LAST_ACK);
        }

        return seq_num_;
    }

}