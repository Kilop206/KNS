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
            bool close_requested = false;
            bool traffic_generated_ = false;

        public:
            TCPSession();

            TCPSession(std::uint64_t session_id,
                        int source,
                        int destination,
                        TCPState state);

            std::uint64_t getSession_id() const;

            int getSource() const;

            int getDestination() const;

            TCPState getState() const;

            TCPConnection& getClientConnection();

            TCPConnection& getServerConnection();

            void incrementPacketsSent();

            void setTotalPackets(int total);

            bool isComplete();

            bool isCloseRequest();

            void setCloseRequest(bool closeRequest);

            bool hasGeneratedTraffic() const noexcept;
            
            void markTrafficGenerated() noexcept;

            void setState(TCPState state);
    };
}