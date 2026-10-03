#include "engine/events/TCPDelayedAckEvent.hpp"

#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns
{
    TCPDelayedAckEvent::TCPDelayedAckEvent(
        double timestamp,
        std::uint64_t session_id,
        int receiver_node,
        std::uint32_t acknowledgement
    )
        : Event(timestamp),
          session_id_(session_id),
          receiver_node_(receiver_node),
          acknowledgement_(acknowledgement)
    {
    }

    void TCPDelayedAckEvent::execute(
        SimulationEngine& engine
    )
    {
        if (!engine.hasTCPSession(session_id_)) {
            return;
        }

        auto& session =
            engine.getTCPSession(session_id_);

        TCPConnection* receiver = nullptr;

        if (
            receiver_node_ ==
            session.getSource()
        ) {
            receiver =
                &session.getClientConnection();
        }
        else if (
            receiver_node_ ==
            session.getDestination()
        ) {
            receiver =
                &session.getServerConnection();
        }
        else {
            return;
        }

        if (!receiver->isEstablished()) {
            return;
        }

        if (
            !receiver->hasDelayedAckPending()
        ) {
            return;
        }

        /*
         * If the expected ACK changed since this event was
         * scheduled, an immediate ACK already acknowledged
         * newer data and this delayed ACK is obsolete.
         */
        if (
            receiver->getExpectedAckNum() !=
            acknowledgement_
        ) {
            receiver->clearDelayedAckPending();
            return;
        }

        Packet ack(
            receiver->getLocalNode(),
            receiver->getRemoteNode(),
            receiver->getLocalNode(),
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        ack.packet_type =
            PacketType::ACK;

        ack.tcp =
            receiver->buildAck();

        ack.departure_time =
            engine.now();

        receiver->clearDelayedAckPending();

        PacketUtils::sendPacketThroughTopology(
            engine,
            ack
        );
    }
}