#include "network/transport/tcp/TCPSession.hpp"

namespace kns {
    TCPSession::TCPSession(std::uint64_t session_id,
                            int source,
                            int destination,
                            TCPState state,
                            std::uint16_t source_port,
                            std::uint16_t destination_port)
                            : session_id(session_id),
                            source(source),
                            destination(destination),
                            client_connection(
                                state, 0, 0, source, destination,
                                CongestionControlType::RENO,
                                TCPConnection::DEFAULT_CONGESTION_MSS,
                                65535,
                                source_port,
                                destination_port
                            ),
                            server_connection(
                                state, 0, 0, destination, source,
                                CongestionControlType::RENO,
                                TCPConnection::DEFAULT_CONGESTION_MSS,
                                65535,
                                destination_port,
                                source_port
                            ) {
    }

    TCPSession::TCPSession()
        : TCPSession(0, 0, 0, TCPState::CLOSED) {
    }

    std::uint64_t TCPSession::getSession_id() const
    {
        return session_id;
    }

    int TCPSession::getSource() const
    {
        return source;
    }

    int TCPSession::getDestination() const
    {
        return destination;
    }

    /// Returns the aggregate TCP state of this session, derived from the
    /// actual states of the client and server connections.
    /// Precedence (highest to lowest):
    ///   ESTABLISHED > CLOSE_WAIT > LAST_ACK > FIN_WAIT_1 > FIN_WAIT_2
    ///   > CLOSING > TIME_WAIT > CLOSED > SYN_RECEIVED > SYN_SENT > LISTEN
    /// Falls back to the client connection state when no explicit rule matches.
    TCPState TCPSession::getState() const
    {
        const TCPState cs = client_connection.getTcpState();
        const TCPState ss = server_connection.getTcpState();

        if (cs == TCPState::ESTABLISHED && ss == TCPState::ESTABLISHED)
            return TCPState::ESTABLISHED;

        if (cs == TCPState::CLOSE_WAIT || ss == TCPState::CLOSE_WAIT)
            return TCPState::CLOSE_WAIT;

        if (cs == TCPState::LAST_ACK || ss == TCPState::LAST_ACK)
            return TCPState::LAST_ACK;

        if (cs == TCPState::FIN_WAIT_1 || ss == TCPState::FIN_WAIT_1)
            return TCPState::FIN_WAIT_1;

        if (cs == TCPState::FIN_WAIT_2 || ss == TCPState::FIN_WAIT_2)
            return TCPState::FIN_WAIT_2;

        if (cs == TCPState::CLOSING || ss == TCPState::CLOSING)
            return TCPState::CLOSING;

        if (cs == TCPState::TIME_WAIT || ss == TCPState::TIME_WAIT)
            return TCPState::TIME_WAIT;

        if (cs == TCPState::CLOSED && ss == TCPState::CLOSED)
            return TCPState::CLOSED;

        if (cs == TCPState::SYN_RECEIVED || ss == TCPState::SYN_RECEIVED)
            return TCPState::SYN_RECEIVED;

        if (cs == TCPState::SYN_SENT || ss == TCPState::SYN_SENT)
            return TCPState::SYN_SENT;

        if (cs == TCPState::LISTEN || ss == TCPState::LISTEN)
            return TCPState::LISTEN;

        // Default: delegate to the client's own state.
        return cs;
    }

    void TCPSession::incrementPacketsSent()
    {
        packets_sent++;
    }

    void TCPSession::setTotalPackets(int total)
    {
        total_packets = total;
    }

    bool TCPSession::isComplete()
    {
        return packets_sent == total_packets;
    }

    bool TCPSession::isCloseRequest()
    {
        return close_requested;
    }

    void TCPSession::setCloseRequest(bool closeRequest)
    {
        this->close_requested = closeRequest;
    }

    bool TCPSession::hasGeneratedTraffic() const noexcept
    {
        return traffic_generated_;
    }

    void TCPSession::markTrafficGenerated() noexcept
    {
        traffic_generated_ = true;
    }

    TCPConnection& TCPSession::getClientConnection() {
        return client_connection;
    }

    TCPConnection& TCPSession::getServerConnection() {
        return server_connection;
    }

    const TCPConnection& TCPSession::getClientConnection() const
    {
        return client_connection;
    }

    const TCPConnection& TCPSession::getServerConnection() const
    {
        return server_connection;
    }
}
