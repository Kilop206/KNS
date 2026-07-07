#include "network/transport/tcp/TCPSession.hpp"

namespace kns {
    TCPSession::TCPSession(std::uint64_t session_id,
                            int source,
                            int destination,
                            TCPState state)
                            : session_id(session_id),
                            source(source),
                            destination(destination),
                            state(state),
                            client_connection(TCPState::CLOSED, 0, 0, source, destination),
                            server_connection(TCPState::CLOSED, 0, 0, destination, source) {
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

    TCPState TCPSession::getState() const
    {
        return state;
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

    void TCPSession::setState(TCPState state)
    {
        this->state = state;
    }

    TCPConnection& TCPSession::getClientConnection() {
        return client_connection;
    }

    TCPConnection& TCPSession::getServerConnection() {
        return server_connection;
    }
}