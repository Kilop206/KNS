#pragma once

#include "enums/TCPState.hpp"

#include <stdint.h>
#include <functional>

namespace kns {
    class TCPConnection {
        private:
            TCPState state;
            int seq_num;
            int expected_ack_num;
            int local_node;
            int remote_node;

            int syn_retries = 0;
            static inline constexpr int MAX_SYN_RETRIES = 3;

        public:
            TCPConnection(TCPState state,
                        int seq_num,
                        int expected_ack_num,
                        int local_node,
                        int remote_node);

            int64_t send_syn();
            void receive_syn(uint32_t remote_seq);

            int64_t send_syn_ack();
            void receive_syn_ack(uint32_t remote_seq, uint32_t remote_ack);

            int64_t send_ack();
            void receive_ack(uint32_t remote_ack);

            void setTcpState(TCPState state);

            int getLocalNode() const;
            int getRemoteNode() const;
            int getSeqNum() const;
            int getExpectedAckNum() const;
            TCPState getTcpState() const;

            void incrementSynRetries();
            bool canRetrySyn() const;
            int getSynRetries() const;
            void resetSynRetries();

        struct TCPConnectionHash
        {
            std::size_t operator()(const TCPConnectionKey& key) const
                {
                    return std::hash<int>()(key.from)
                         ^ (std::hash<int>()(key.to) << 1);
                }
            };

            struct TCPConnectionEqual
            {
                bool operator()(
                    const TCPConnection& a,
                    const TCPConnection& b
                ) const
                {
                    return a.from == b.from
                        && a.to == b.to;
                }
            };
        };
}