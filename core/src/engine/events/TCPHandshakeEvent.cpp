#include "engine/events/TCPHandshakeEvent.hpp"
#include "network/utils/PacketUtils.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "network/Packet.hpp"
#include "enums/PacketType.hpp"
#include "enums/TCPState.hpp"
#include "network/Link.hpp"
#include "network/transport/tcp/TCPSession.hpp"

#include <iostream>

namespace kns {

    TCPHandshakeEvent::TCPHandshakeEvent(double timestamp,
                                        int source,
                                        int destination,
                                        std::uint64_t session_id)
        : Event(timestamp),
        source_(source),
        destination_(destination),
        session_id(session_id) {}

    void TCPHandshakeEvent::execute(SimulationEngine& engine) {

        TCPSession tcpSession(session_id, source_, destination_, TCPState::ESTABLISHED);

        TCPConnection client(
            TCPState::CLOSED,
            0,
            0,
            source_,
            destination_
        );

        TCPConnection server(
            TCPState::CLOSED,
            0,
            0,
            destination_,
            source_
        );

        const int client_seq = client.send_syn();

        Packet syn(
            source_,
            destination_,
            source_,
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id
        );
        syn.packet_type = PacketType::SYN;
        syn.seq_num = client_seq;
        syn.ack_num = 0;
        syn.departure_time = engine.now();

        if (!sendPacketThroughTopology(engine, syn)) {
            return;
        }

        server.receive_syn(client_seq);
    }

}
