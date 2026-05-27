#include "network/tcp/TCPSession.hpp"

namespace kns
{
    TCPSession::TCPSession(std::uint64_t sessionId,
                            int source,
                            int destination,
                            TCPState state
                            ) {}

    std::uint64_t TCPSession::getSession_id()
    {
        return session_id;
    }

    int TCPSession::getSource()
    {
        return source;
    }

    int TCPSession::getDestination()
    {
        return destination;
    }

    TCPState TCPSession::getState()
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
        return packetsSent == totalPackets;
    }

    void TCPSession::setState(TCPState state)
    {
        this->state = state;
    }
}