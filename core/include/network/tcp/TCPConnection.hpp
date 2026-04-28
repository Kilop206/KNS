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

            int getLocalNode() const;

            int getRemoteNode() const;
    };

    struct TCPConnectionHash {
        std::size_t operator()(const TCPConnection& c) const {
            return std::hash<int>()(c.getLocalNode()) ^
                (std::hash<int>()(c.getRemoteNode()) << 1);
        }
    };

    struct TCPConnectionEqual {
        bool operator()(const TCPConnection& a, const TCPConnection& b) const {
            return a.getLocalNode() == b.getLocalNode() &&
                a.getRemoteNode() == b.getRemoteNode();
        }
    };
}