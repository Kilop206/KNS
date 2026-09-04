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
          send_unacknowledged_(seq_num),
          send_window_(DEFAULT_SEND_WINDOW),
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

        updateSendUnacknowledged(remote_ack);

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

            updateSendUnacknowledged(remote_ack);

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

        if (!isEstablished()) {
            return false;
        }

        if (remote_ack <= send_unacknowledged_) {
            return false;
        }

        if (remote_ack > seq_num_) {
            return false;
        }

        acknowledgeSentData(remote_ack);
        updateSendUnacknowledged(remote_ack);

        return true;
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
            return true;
        }

        if (!state_machine_.onSynSent()) {
            return false;
        }

        seq_num_ = generateInitialSeq();

        send_unacknowledged_ = seq_num_;

        return true;
    }

    bool TCPConnection::send_syn_ack()
    {
        return getTcpState() == TCPState::SYN_RECEIVED;
    }

    bool TCPConnection::send_ack()
    {
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

    std::uint32_t TCPConnection::getSendUnacknowledged() const noexcept
    {
        return send_unacknowledged_;
    }

    std::uint32_t TCPConnection::getSendNext() const noexcept
    {
        return seq_num_;
    }

    std::uint32_t TCPConnection::getSendWindow() const noexcept
    {
        return send_window_;
    }

    void TCPConnection::setSendWindow(
        std::uint32_t window
    ) noexcept
    {
        send_window_ = window;
    }

    std::size_t TCPConnection::getSendBufferSize() const noexcept
    {
        return send_buffer_.size();
    }

    bool TCPConnection::canSend(
        std::size_t payload_size
    ) const noexcept
    {
        const std::uint32_t in_flight =
            seq_num_ - send_unacknowledged_;

        if (payload_size >
            static_cast<std::size_t>(send_window_)) {
            return false;
        }

        if (
            in_flight >
            send_window_ -
                static_cast<std::uint32_t>(
                    payload_size
                )
        ) {
            return false;
        }

        return true;
    }

    bool TCPConnection::queueSentSegment(
        const TCPSegment& segment,
        double sent_at
    )
    {
        if (segment.payloadSize() == 0) {
            return false;
        }

        if (segment.seq != seq_num_) {
            return false;
        }

        if (!canSend(segment.payloadSize())) {
            return false;
        }

        TCPSendEntry entry{
            segment,
            sent_at
        };

        if (!send_buffer_.push(std::move(entry))) {
            return false;
        }

        seq_num_ +=
            static_cast<std::uint32_t>(
                segment.payloadSize()
            );

        return true;
    }

    std::size_t TCPConnection::acknowledgeSentData(
        std::uint32_t ack_number
    )
    {
        return send_buffer_.acknowledge(ack_number);
    }

    void TCPConnection::updateSendUnacknowledged(
        std::uint32_t ack_number
    ) noexcept
    {
        if (ack_number > send_unacknowledged_) {
            send_unacknowledged_ = ack_number;
        }
    }

}