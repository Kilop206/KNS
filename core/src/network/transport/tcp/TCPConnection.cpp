#include "network/transport/tcp/TCPConnection.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/Random.hpp"

namespace kns {

    std::uint32_t TCPConnection::generateInitialSeq()
    {
        return Random::nextUint32();
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

    bool TCPConnection::receive_syn(std::uint32_t remote_seq)
    {
        if (!state_machine_.onSynReceived()) {
            return false;
        }

        expected_ack_num_ = remote_seq + 1;

        KNS_DEBUG_LOG(
            "[TCP SYN] "
            << "local=" << local_node_
            << " remote=" << remote_node_
            << " local_seq=" << seq_num_
            << " remote_seq=" << remote_seq
            << " expected_ack=" << expected_ack_num_
            << '\n');

        return true;
    }

    bool TCPConnection::receive_syn_ack(
        std::uint32_t remote_seq,
        std::uint32_t remote_ack
    )
    {
        KNS_DEBUG_LOG(
            "[TCP SYN_ACK] "
            << "local=" << local_node_
            << " remote=" << remote_node_
            << " local_seq=" << seq_num_
            << " remote_seq=" << remote_seq
            << " remote_ack=" << remote_ack
            << " expected_ack=" << (seq_num_ + 1)
            << " state=" << static_cast<int>(getTcpState())
            << '\n');

        if (getTcpState() != TCPState::SYN_SENT) {
            return false;
        }

        if (remote_ack != seq_num_ + 1) {
            return false;
        }

        if (!state_machine_.onEstablished()) {
            return false;
        }

        expected_ack_num_ = remote_seq + 1;
        resetSynRetries();

        return true;
    }

    bool TCPConnection::receive_ack(std::uint32_t remote_ack)
    {
        KNS_DEBUG_LOG(
            "[TCP ACK] "
            << "local=" << local_node_
            << " remote=" << remote_node_
            << " state=" << static_cast<int>(getTcpState())
            << " local_seq=" << seq_num_
            << " remote_ack=" << remote_ack
            << " expected=" << (seq_num_ + 1)
            << '\n');

        if (
            getTcpState() == TCPState::SYN_RECEIVED &&
            remote_ack == seq_num_ + 1
        ) {
            KNS_DEBUG_LOG("[TCP ACK] SERVER -> ESTABLISHED\n");

            if (!state_machine_.onEstablished()) {
                return false;
            }

            resetSynRetries();

            return true;
        }

        if (
            getTcpState() == TCPState::FIN_WAIT_1 &&
            remote_ack == seq_num_ + 1
        ) {
            return state_machine_.onFinAcked();
        }

        if (
            getTcpState() == TCPState::LAST_ACK &&
            remote_ack == seq_num_ + 1
        ) {
            return state_machine_.onFinAcked();
        }

        return false;
    }

    bool TCPConnection::receive_fin(std::uint32_t remote_seq)
    {
        if (!state_machine_.onPeerFin()) {
            return false;
        }

        expected_ack_num_ = remote_seq + 1;
        return true;
    }

    bool TCPConnection::send_syn()
    {
        if (getTcpState() == TCPState::SYN_SENT) {
            // Already in SYN_SENT (retransmit path) — seq is already set.
            return true;
        }

        if (!state_machine_.onSynSent()) {
            return false;
        }

        seq_num_ = generateInitialSeq();
        return true;
    }

    bool TCPConnection::send_syn_ack()
    {
        // The server transitions to SYN_RECEIVED when it receives a SYN
        // (receive_syn()), not when it sends the SYN-ACK. So this method only
        // validates that we are already in SYN_RECEIVED before allowing the
        // segment to be sent — no state change needed.
        return getTcpState() == TCPState::SYN_RECEIVED;
    }

    bool TCPConnection::send_ack()
    {
        // Sending a plain ACK is valid from many states (handshake, data,
        // close). This is a pure segment construction call — it does NOT drive
        // the state machine. The state machine is advanced by receive_*().
        return true;
    }

    bool TCPConnection::send_fin()
    {
        return state_machine_.onFinSent();
    }

    bool TCPConnection::expire_time_wait() noexcept
    {
        return state_machine_.onTimeWaitDone();
    }

}
