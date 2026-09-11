#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <vector>

#include "enums/TCPState.hpp"
#include "network/transport/tcp/TCPSegment.hpp"
#include "network/transport/tcp/TCPStateMachine.hpp"
#include "network/transport/tcp/buffer/TCPReceiveBuffer.hpp"
#include "network/transport/tcp/buffer/TCPSendBuffer.hpp"
#include "network/transport/tcp/congestion/CongestionControl.hpp"
#include "network/transport/tcp/congestion/CongestionControlType.hpp"
#include "network/transport/tcp/congestion/TcpCongestionSample.hpp"
#include "network/transport/tcp/recovery/TCPLossDetector.hpp"
#include "network/transport/tcp/timer/RTOManager.hpp"

namespace kns {

    class TCPConnection {
    public:
        static constexpr std::uint32_t MAX_SYN_RETRIES = 5;
        static constexpr std::uint32_t DEFAULT_SEND_WINDOW = 65535;
        static constexpr std::size_t DEFAULT_RECEIVE_WINDOW = 65535;
        static constexpr std::uint32_t MAX_DATA_RETRANSMISSIONS = 5;
        static constexpr std::uint32_t DEFAULT_CONGESTION_MSS = DEFAULT_SEND_WINDOW;

        TCPConnection(
            TCPState state,
            std::uint32_t seq_num,
            std::uint32_t expected_ack_num,
            int local_node,
            int remote_node,
            CongestionControlType congestion_control_type =
                CongestionControlType::RENO,
            std::uint32_t congestion_mss =
                DEFAULT_CONGESTION_MSS,
            std::uint32_t congestion_initial_ssthresh =
                65535,
            std::uint16_t local_port = 0,
            std::uint16_t remote_port = 0
        );

        TCPState getTcpState() const noexcept;

        bool onListen() noexcept;

        int getLocalNode() const noexcept;
        int getRemoteNode() const noexcept;
        std::uint16_t getLocalPort() const noexcept;
        std::uint16_t getRemotePort() const noexcept;

        std::uint32_t getSeqNum() const noexcept;
        void setSeqNum(std::uint32_t value) noexcept;

        std::uint32_t getExpectedAckNum() const noexcept;
        void setExpectedAckNum(std::uint32_t value) noexcept;

        std::uint32_t getSynRetries() const noexcept;
        void incrementSynRetries() noexcept;
        bool canRetrySyn() const noexcept;
        void resetSynRetries() noexcept;

        bool isEstablished() const noexcept;
        bool isClosed() const noexcept;

        TCPSegment buildSyn() const;
        TCPSegment buildSynAck() const;
        TCPSegment buildAck() const;
        TCPSegment buildFin() const;
        TCPSegment buildRst(std::uint32_t remote_seq) const;

        bool receive_syn(std::uint32_t remote_seq);

        bool receive_syn_ack(
            std::uint32_t remote_seq,
            std::uint32_t remote_ack
        );

        bool receive_ack(
            std::uint32_t remote_ack,
            double acknowledgement_time
        );

        bool receive_fin(std::uint32_t remote_seq);

        bool send_syn();

        bool markSynSent() noexcept;

        bool send_syn_ack();

        bool send_ack();

        bool send_fin();

        bool expire_time_wait() noexcept;

        std::uint32_t getSendUnacknowledged() const noexcept;
        std::uint32_t getSendNext() const noexcept;
        std::uint32_t getSendWindow() const noexcept;

        void setSendWindow(
            std::uint32_t window
        ) noexcept;

        std::size_t getSendBufferSize() const noexcept;

        bool canSend(
            std::size_t payload_size
        ) const noexcept;

        bool queueSentSegment(
            const TCPSegment& segment,
            double sent_at
        );

        std::size_t acknowledgeSentData(
            std::uint32_t ack_number
        );

        std::size_t getReceiveBufferSize() const noexcept;

        std::size_t getReceiveBufferedBytes() const noexcept;

        std::optional<std::uint32_t>
            getOldestOutstandingSequence() const noexcept;

        std::size_t getReceiveWindow() const noexcept;

        bool receive_data(
            std::uint32_t sequence,
            const std::vector<std::uint8_t>& payload,
            double received_at
        );

        bool hasOutstandingSegment(
            std::uint32_t sequence
        ) const noexcept;

        double getCurrentRTO() const noexcept;

        void onSendTimeout(
            double timeout_time
        ) noexcept;

        void onSendTimeout() noexcept;

        void onAcknowledged(
            std::uint32_t ack_number,
            double acknowledgement_time
        ) noexcept;

        std::optional<TCPSegment> getOutstandingSegment(
            std::uint32_t sequence
        ) const;

        bool markSegmentRetransmitted(
            std::uint32_t sequence,
            double retransmission_time
        ) noexcept;

        std::uint32_t getRetransmissionCount(
            std::uint32_t sequence
        ) const noexcept;

        bool canRetransmit(
            std::uint32_t sequence
        ) const noexcept;

        bool failRetransmission() noexcept;

        /*
         * Duplicate ACK / fast retransmit detection.
         */
        std::uint32_t getDuplicateAckCount() const noexcept;

        bool shouldFastRetransmit() const noexcept;

        void resetLossDetection() noexcept;

        TCPStateMachine getStateMachine() const noexcept;

        bool hasDelayedAckPending() const noexcept;

        void markDelayedAckPending() noexcept;

        void clearDelayedAckPending() noexcept;

        CongestionControl&
        getCongestionControl() noexcept;

        const CongestionControl&
        getCongestionControl() const noexcept;

        CongestionControlType
        getCongestionControlType() const noexcept;

        const std::vector<TcpCongestionSample>&
        getCongestionHistory() const noexcept;

        void onFastRetransmit(
            std::uint32_t flight_size,
            double timestamp
        ) noexcept;

        void recordCongestionSample(
            double timestamp
        ) noexcept;

    private:
        static std::uint32_t generateInitialSeq();

        void updateSendUnacknowledged(
            std::uint32_t ack_number
        ) noexcept;

    private:
        RTOManager rto_manager_;

        TCPStateMachine state_machine_;

        std::uint32_t seq_num_;
        std::uint32_t expected_ack_num_;

        std::uint32_t send_unacknowledged_;
        std::uint32_t send_window_;

        int local_node_;
        int remote_node_;
        std::uint16_t local_port_;
        std::uint16_t remote_port_;

        std::uint32_t syn_retries_ = 0;

        TCPSendBuffer send_buffer_;
        TCPReceiveBuffer receive_buffer_;

        TCPLossDetector loss_detector_;

        bool delayed_ack_pending_ = false;

        std::unique_ptr<CongestionControl> congestion_control_;

        CongestionControlType congestion_control_type_;

        std::vector<TcpCongestionSample> congestion_history_;
    };
}
