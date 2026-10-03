#include "engine/events/TCPHandshakeEvent.hpp"

<<<<<<< HEAD
#include "network/utils/PacketUtils.hpp"
#include "network/tcp/TCPConnection.hpp"
#include "network/Packet.hpp"
#include "enums/PacketType.hpp"

namespace kns {

TCPHandshakeEvent::TCPHandshakeEvent(double timestamp, int source, int destination)
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

    const int server_seq = server.send_syn_ack();

    Packet syn_ack(
        destination_,
        source_,
        destination_,
        engine.now(),
        engine.getGlobalPacketSize()
    );
    syn_ack.packet_type = PacketType::SYN_ACK;
    syn_ack.seq_num = server_seq;
    syn_ack.ack_num = client_seq + 1;
    syn_ack.departure_time = engine.now();

    if (!sendPacketThroughTopology(engine, syn_ack)) {
        return;
    }

    client.receive_syn_ack(server_seq, client_seq + 1);

    const int client_ack = client.send_ack();

    Packet ack(
        source_,
        destination_,
        source_,
        engine.now(),
        engine.getGlobalPacketSize()
    );
    ack.packet_type = PacketType::ACK;
    ack.seq_num = client_seq + 1;
    ack.ack_num = client_ack;
    ack.departure_time = engine.now();

    if (!sendPacketThroughTopology(engine, ack)) {
        return;
    }

    server.receive_ack(client_ack);
}

}
=======
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPHandshakeTimeoutEvent.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPConnection.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

#include "engine/core/Log.hpp"

namespace kns {

    TCPHandshakeEvent::TCPHandshakeEvent(
        double timestamp,
        int source,
        int destination,
        std::uint64_t session_id
    )
        : Event(timestamp),
          source_(source),
          destination_(destination),
          session_id_(session_id)
    {
    }

    void TCPHandshakeEvent::execute(SimulationEngine& engine)
    {
        KNS_DEBUG_LOG(
            "[TCP] Handshake session "
            << session_id_
            << '\n');

        auto& session = engine.getTCPSession(session_id_);
        auto& client = session.getClientConnection();

        const auto initial_seq = client.getTcpState() == TCPState::SYN_SENT
            ? client.getSeqNum() : engine.randomSequence();
        if (!client.send_syn(initial_seq)) {
            KNS_DEBUG_LOG(
                "[TCP][SESSION "
                << session_id_
                << "] send_syn() rejected in state "
                << static_cast<int>(client.getTcpState())
                << " — handshake aborted\n");
            return;
        }

        Packet syn(
            source_,
            destination_,
            source_,
            engine.now(),
            engine.getGlobalPacketSize(),
            session_id_
        );

        syn.tcp = client.buildSyn();
        syn.packet_type = inferPacketType(syn.tcp);
        syn.departure_time = engine.now();

        // The timer tracks the handshake, even when the network drops the SYN.
        PacketUtils::sendPacketThroughTopology(engine, syn);
        {
            engine.schedule(
                std::make_unique<TCPHandshakeTimeoutEvent>(
                    engine.now() + 1.0,
                    session_id_
                )
            );

            KNS_DEBUG_LOG(
                "[TCP][SESSION "
                << session_id_
                << "] SYN timeout scheduled at "
                << engine.now() + 1.0
                << '\n');
        }
    }
}
>>>>>>> 879e9a30eb706359e007b3218a4c881c257cd5bc
