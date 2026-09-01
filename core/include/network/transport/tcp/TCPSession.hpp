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
            /// @deprecated Kept for legacy callers (e.g. TCPTimeWaitTimeoutEvent).
            /// getState() no longer reads this field — it derives state from the
            /// two TCPConnection objects. setState() still writes it for callers
            /// that rely on the side-effect, but should be removed once all
            /// callers are migrated to inspect the connections directly.
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

            /// Returns the aggregate state derived from the client and server
            /// connection states. See TCPSession.cpp for the derivation rules.
            TCPState getState() const;

            TCPConnection& getClientConnection();
            const TCPConnection& getClientConnection() const;

            TCPConnection& getServerConnection();
            const TCPConnection& getServerConnection() const;

            void incrementPacketsSent();

            void setTotalPackets(int total);

            bool isComplete();

            bool isCloseRequest();

            void setCloseRequest(bool closeRequest);

            bool hasGeneratedTraffic() const noexcept;
            
            void markTrafficGenerated() noexcept;

            /// @deprecated setState() no longer affects getState(). Kept for
            /// transition compatibility while callers are migrated.
            void setState(TCPState state);
    };
}