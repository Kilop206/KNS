#include "network/transport/tcp/TCPConnection.hpp"

#include <algorithm>
#include <utility>

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
          remote_node_(remote_node),
          receive_buffer_(
              expected_ack_num,
              DEFAULT_RECEIVE_WINDOW
          )
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

        receive_buffer_.setNextSequence(value);
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
        seg.window = static_cast<std::uint16_t>(
            std::min<std::size_t>(
                receive_buffer_.availableWindow(),
                65535
            )
        );
        seg.flags = TCPFlag::SYN;

        return seg;
    }

    TCPSegment TCPConnection::buildSynAck() const
    {
        TCPSegment seg;

        seg.seq = seq_num_;
        seg.ack = expected_ack_num_;
        seg.window = static_cast<std::uint16_t>(
            std::min<std::size_t>(
                receive_buffer_.availableWindow(),
                65535
            )
        );
        seg.flags = TCPFlag::SYN | TCPFlag::ACK;

        return seg;
    }

    TCPSegment TCPConnection::buildAck() const
    {
        TCPSegment seg;

        seg.seq = seq_num_;
        seg.ack = expected_ack_num_;
        seg.window = static_cast<std::uint16_t>(
            std::min<std::size_t>(
                receive_buffer_.availableWindow(),
                65535
            )
        );
        seg.flags = TCPFlag::ACK;

        return seg;
    }

    TCPSegment TCPConnection::buildFin() const
    {
        TCPSegment seg;

        seg.seq = seq_num_;
        seg.ack = expected_ack_num_;
        seg.window = static_cast<std::uint16_t>(
            std::min<std::size_t>(
                receive_buffer_.availableWindow(),
                65535
            )
        );
        seg.flags = TCPFlag::FIN | TCPFlag::ACK;

        return seg;
    }

    bool TCPConnection::receive_syn(std::uint32_t remote_seq)
    {
        if (!state_machine_.onSynReceived()) {
            return false;
        }

        expected_ack_num_ = remote_seq + 1;

        receive_buffer_.setNextSequence(
            expected_ack_num_
        );

        KNS_DEBUG_LOG(
            "[TCP SYN] "
            << "local=" << local_node_
            << " remote=" << remote_node_
            << " local_seq=" << seq_num_
            << " remote_seq=" << remote_seq
            << " expected_ack=" << expected_ack_num_
            << '\n'
        );

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
            << '\n'
        );

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

        receive_buffer_.setNextSequence(
            expected_ack_num_
        );

        // The SYN consumes one sequence number.
        seq_num_ = remote_ack;
        send_unacknowledged_ = remote_ack;

        resetSynRetries();

        return true;
    }

    bool TCPConnection::receive_ack(
        std::uint32_t remote_ack,
        double acknowledgement_time
    )
    {
        KNS_DEBUG_LOG(
            "[TCP ACK] "
            << "local=" << local_node_
            << " remote=" << remote_node_
            << " state=" << static_cast<int>(getTcpState())
            << " local_seq=" << seq_num_
            << " remote_ack=" << remote_ack
            << " expected=" << (seq_num_ + 1)
            << '\n'
        );

        if (
            getTcpState() == TCPState::SYN_RECEIVED &&
            remote_ack == seq_num_ + 1
        ) {
            KNS_DEBUG_LOG(
                "[TCP ACK] SERVER -> ESTABLISHED\n"
            );

            if (!state_machine_.onEstablished()) {
                return false;
            }

            seq_num_ = remote_ack;
            send_unacknowledged_ = remote_ack;

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

        onAcknowledged(
            remote_ack,
            acknowledgement_time
        );

        return true;
    }

    bool TCPConnection::receive_fin(
        std::uint32_t remote_seq
    )
    {
        if (!state_machine_.onPeerFin()) {
            return false;
        }

        expected_ack_num_ = remote_seq + 1;

        receive_buffer_.setNextSequence(
            expected_ack_num_
        );

        return true;
    }

    bool TCPConnection::send_syn()
    {
        if (getTcpState() == TCPState::SYN_SENT) {
            // Already in SYN_SENT (retransmit path).
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
        // The server transitions to SYN_RECEIVED when it receives
        // the SYN. Sending the SYN-ACK does not change state.
        return getTcpState() == TCPState::SYN_RECEIVED;
    }

    bool TCPConnection::send_ack()
    {
        // Plain ACK generation does not drive the state machine.
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

        if (
            payload_size >
            static_cast<std::size_t>(send_window_)
        ) {
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

        seq_num_ += static_cast<std::uint32_t>(
            segment.payloadSize()
        );

        return true;
    }

    std::size_t TCPConnection::acknowledgeSentData(
        std::uint32_t ack_number
    )
    {
        return send_buffer_.acknowledge(
            ack_number
        );
    }

    void TCPConnection::updateSendUnacknowledged(
        std::uint32_t ack_number
    ) noexcept
    {
        if (ack_number > send_unacknowledged_) {
            send_unacknowledged_ = ack_number;
        }
    }

    bool TCPConnection::receive_data(
        std::uint32_t sequence,
        const std::vector<std::uint8_t>& payload,
        double received_at
    )
    {
        if (payload.empty()) {
            return false;
        }

        TCPSegment segment;

        segment.seq = sequence;
        segment.ack = expected_ack_num_;
        segment.window = 0;
        segment.flags = TCPFlag::ACK | TCPFlag::PSH;
        segment.payload = payload;

        TCPReceiveEntry entry{
            segment,
            received_at
        };

        if (!receive_buffer_.push(std::move(entry))) {
            return false;
        }

        receive_buffer_.consumeContiguous();

        expected_ack_num_ =
            receive_buffer_.nextSequence();

        return true;
    }

    std::size_t TCPConnection::getReceiveBufferSize()
        const noexcept
    {
        return receive_buffer_.size();
    }

    std::size_t TCPConnection::getReceiveBufferedBytes()
        const noexcept
    {
        return receive_buffer_.bufferedBytes();
    }

    std::size_t TCPConnection::getReceiveWindow()
        const noexcept
    {
        return receive_buffer_.availableWindow();
    }

    bool TCPConnection::hasOutstandingSegment(
        std::uint32_t sequence
    ) const noexcept
    {
        return send_buffer_.contains(sequence);
    }

    double TCPConnection::getCurrentRTO() const noexcept
    {
        return rto_manager_.currentRTO();
    }

    void TCPConnection::onSendTimeout() noexcept
    {
        rto_manager_.onTimeout();
    }

    void TCPConnection::onAcknowledged(
        std::uint32_t ack_number,
        double acknowledgement_time
    ) noexcept
    {
        const auto sample =
            send_buffer_.acknowledgeAndGetRtt(
                ack_number,
                acknowledgement_time
            );

        updateSendUnacknowledged(
            ack_number
        );

        if (sample.has_value()) {
            rto_manager_.onAcknowledgement(
                *sample
            );
        }
    }

    std::optional<TCPSegment>
    TCPConnection::getOutstandingSegment(
        std::uint32_t sequence
    ) const
    {
        const TCPSendEntry* entry =
            send_buffer_.find(sequence);

        if (entry == nullptr) {
            return std::nullopt;
        }

        return entry->segment;
    }

    bool TCPConnection::markSegmentRetransmitted(
        std::uint32_t sequence,
        double retransmission_time
    ) noexcept
    {
        return send_buffer_.markRetransmitted(
            sequence,
            retransmission_time
        );
    }
}