#pragma once

#include <cstdint>

#include "enums/TCPState.hpp"
#include "network/transport/tcp/TCPConnection.hpp"

namespace kns
{

    class TCPSession
    {
        private:
            std::uint64_t session_id;
            int source;
            int destination;
            TCPState state;
            int total_packets = 0;
            int packets_sent = 0;
            TCPConnection client_connection;
            TCPConnection server_connection;

        public:
            TCPSession(std::uint64_t session_id,
                        int source,
                        int destination,
                        TCPState state);

            std::uint64_t getSession_id();

            int getSource();

            int getDestination();

            TCPState getState();

            void incrementPacketsSent();

            void setTotalPackets(int total);

            bool isComplete();

            void setState(TCPState state);
    };
}