#include "engine/events/TCPHandshakeEvent.hpp"
#include "engine/events/TCPSynAckEvent.hpp"
#include "network/utils/PacketUtils.hpp"
#include "network/tcp/TCPConnection.hpp"
#include "network/Packet.hpp"
#include "enums/PacketType.hpp"
#include "network/Link.hpp"

#include <iostream>

namespace kns {

TCPHandshakeEvent::TCPHandshakeEvent(double timestamp,
                                    int source,
                                    int destination)
    : Event(timestamp),
      source_(source),
      destination_(destination) {}

void TCPHandshakeEvent::execute(SimulationEngine& engine) {

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
        engine.getGlobalPacketSize()
    );
    syn.packet_type = PacketType::SYN;
    syn.seq_num = client_seq;
    syn.ack_num = 0;
    syn.departure_time = engine.now();

    if (!sendPacketThroughTopology(engine, syn)) {
        return;
    }

    server.receive_syn(client_seq);

    for (Link link : engine.getTopology().getLinksFromNode(source_)) {
        if (link.getOtherNode(source_) == engine.getNextHop(source_, destination_)) {
            TCPSynAckEvent syn_ack(engine.compute_arrival_time(syn, link, engine.now()), source_, destination_, syn.seq_num, syn.ack_num);
            engine.schedule(std::make_unique<TCPSynAckEvent>(syn_ack));
            break;
        }
    }
}

}