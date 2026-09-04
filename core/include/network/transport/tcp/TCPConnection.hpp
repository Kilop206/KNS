#pragma once

#include <cstdint>

#include "enums/TCPState.hpp"
#include "network/transport/tcp/TCPSegment.hpp"
#include "network/transport/tcp/TCPStateMachine.hpp"

namespace kns {

    class TCPConnection {
        public:
            static constexpr std::uint32_t MAX_SYN_RETRIES = 5;

            TCPConnection(
                TCPState state,
                std::uint32_t seq_num,
                std::uint32_t expected_ack_num,
                int local_node,
                int remote_node
            );

            TCPState getTcpState() const noexcept;

            int getLocalNode() const noexcept;
            int getRemoteNode() const noexcept;

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

            bool receive_syn(std::uint32_t remote_seq);
            bool receive_syn_ack(std::uint32_t remote_seq, std::uint32_t remote_ack);
            bool receive_ack(std::uint32_t remote_ack);
            bool receive_fin(std::uint32_t remote_seq);

            /// Transition to SYN_SENT and record the initial seq.
            /// Returns false if the current state does not allow sending a SYN.
            bool send_syn();

            /// Validate that a SYN-ACK may be sent.
            /// The connection must already be in SYN_RECEIVED.
            /// Returns false otherwise; does not change the state.
            bool send_syn_ack();

            /// Pure segment construction — does NOT change state.
            /// Call after receive_syn_ack() / receive_syn() confirmed the handshake.
            /// Always returns true.
            bool send_ack();

            /// Transition to FIN_WAIT_1 (ESTABLISHED) or LAST_ACK (CLOSE_WAIT).
            /// Returns false if the current state does not allow sending a FIN.
            bool send_fin();

            bool expire_time_wait() noexcept;

        private:
            static std::uint32_t generateInitialSeq();

        private:
            TCPStateMachine state_machine_;
            std::uint32_t seq_num_;
            std::uint32_t expected_ack_num_;
            int local_node_;
            int remote_node_;
            std::uint32_t syn_retries_ = 0;
    };

}
