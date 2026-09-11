#include "engine/events/PacketReceivedEvent.hpp"

#include <cassert>
#include <memory>
#include <utility>

#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPConnectionCloseEvent.hpp"
#include "engine/events/TCPDelayedAckEvent.hpp"
#include "engine/events/TCPFastRetransmitEvent.hpp"
#include "engine/events/TCPTimeoutEvent.hpp"
#include "engine/events/TCPTimeWaitTimeoutEvent.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPListener.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns
{
    namespace
    {
        bool packetMatchesSessionEndpoints(
            const Packet& packet,
            const TCPSession& session
        )
        {
            const int source = session.getSource();
            const int destination = session.getDestination();

            const auto& client = session.getClientConnection();
            const auto& server = session.getServerConnection();

            const bool client_to_server =
                packet.source == source &&
                packet.destination == destination &&
                packet.tcp.source_port == client.getLocalPort() &&
                packet.tcp.destination_port == client.getRemotePort();

            const bool server_to_client =
                packet.source == destination &&
                packet.destination == source &&
                packet.tcp.source_port == server.getLocalPort() &&
                packet.tcp.destination_port == server.getRemotePort();

            return client_to_server || server_to_client;
        }
    } // namespace

    PacketReceivedEvent::PacketReceivedEvent(double timestamp, Packet packet)
        : Event(timestamp),
          packet(std::move(packet))
    {
    }

    void PacketReceivedEvent::execute(SimulationEngine& engine)
    {
        // Release the slot in the previous link (if any) as the packet has left the link
        PacketUtils::releasePacketThroughTopology(engine, packet);

        packet.packet_type = inferPacketType(packet.tcp);

        assert(packet.current_node >= 0);

        if (packet.current_node != packet.destination) {
            auto& stats = engine.getStats();

            if (!PacketUtils::sendPacketThroughTopology(engine, packet)) {
                stats.packets_lost++;
            }

            return;
        }

        auto& stats = engine.getStats();
        stats.packets_delivered++;

        if (packet.packet_type == PacketType::DATA) {
            const double latency = engine.now() - packet.creation_time;
            stats.total_latency += latency;
            engine.notifyLatencyDelivered(latency);
        }

        // A packet must match both its correlation ID and the session's
        // endpoint pair before it can be dispatched to that session.
        const bool has_matching_session =
            engine.hasTCPSession(packet.session_id) &&
            packetMatchesSessionEndpoints(
                packet,
                engine.getTCPSession(packet.session_id)
            );

        if (!has_matching_session) {
            if (packet.packet_type == PacketType::RST) {
                return;
            }

            if (packet.packet_type != PacketType::SYN) {
                return;
            }

            // acceptOnListener returns INVALID_SESSION_ID for both an absent
            // listener and a listener that cannot accept another connection.
            const std::uint64_t new_sid =
                engine.acceptOnListener(
                    packet.destination,
                    packet.source,
                    packet.tcp.seq,
                    packet.tcp.source_port,
                    packet.tcp.destination_port
                );

            if (new_sid == TCPListener::INVALID_SESSION_ID) {
                PacketUtils::sendReset(
                    engine,
                    packet.destination,
                    packet.source,
                    packet.tcp.seq,
                    packet.session_id,
                    packet.tcp.destination_port,
                    packet.tcp.source_port
                );

                return;
            }

            packet.session_id = new_sid;
        }

        auto& session = engine.getTCPSession(packet.session_id);
        auto& client = session.getClientConnection();
        auto& server = session.getServerConnection();

        auto& receiver =
            packet.destination == session.getSource() ? client : server;

        switch (packet.packet_type) {
            case PacketType::SYN: {

                if (!receiver.receive_syn(packet.tcp.seq)) {
                    break;
                }

                Packet synAck(
                    receiver.getLocalNode(),
                    receiver.getRemoteNode(),
                    receiver.getLocalNode(),
                    engine.now(),
                    engine.getGlobalPacketSize(),
                    packet.session_id
                );

                synAck.tcp = receiver.buildSynAck();
                synAck.packet_type = inferPacketType(synAck.tcp);

                PacketUtils::sendPacketThroughTopology(engine, synAck);
                break;
            }

            case PacketType::SYN_ACK: {

                if (!client.receive_syn_ack(packet.tcp.seq, packet.tcp.ack)) {
                    break;
                }

                Packet ack(
                    client.getLocalNode(),
                    client.getRemoteNode(),
                    client.getLocalNode(),
                    engine.now(),
                    engine.getGlobalPacketSize(),
                    packet.session_id
                );

                ack.tcp = client.buildAck();
                ack.packet_type = inferPacketType(ack.tcp);

                PacketUtils::sendPacketThroughTopology(engine, ack);
                break;
            }

            case PacketType::ACK:
            {
                const bool acknowledged =
                    receiver.receive_ack(
                        packet.tcp.ack,
                        engine.now()
                    );

                receiver.getCongestionControl().onDuplicateAck();

                if (acknowledged) {
                    const auto oldest =
                        receiver.getOldestOutstandingSequence();

                    if (oldest.has_value()) {
                        engine.schedule(
                            std::make_unique<TCPTimeoutEvent>(
                                engine.now() +
                                    receiver.getCurrentRTO(),
                                session.getSession_id(),
                                *oldest
                            )
                        );
                    }
                }

                /*
                * Three duplicate ACKs indicate a probable
                * loss of the oldest outstanding segment.
                *
                * The event performs the actual retransmission.
                */
                if (receiver.shouldFastRetransmit()) {
                    engine.schedule(
                        std::make_unique<TCPFastRetransmitEvent>(
                            engine.now(),
                            session.getSession_id()
                        )
                    );

                    /*
                    * Consume the current duplicate-ACK indication
                    * so that the same ACK streak cannot schedule
                    * another fast retransmission event.
                    */
                    receiver.resetLossDetection();
                }

                if (
                    client.getTcpState() == TCPState::ESTABLISHED &&
                    server.getTcpState() == TCPState::ESTABLISHED &&
                    !session.hasGeneratedTraffic()
                ) {
                    engine.generatePackets(engine.now(), session);
                }

                if (
                    client.getTcpState() == TCPState::ESTABLISHED &&
                    server.getTcpState() == TCPState::ESTABLISHED &&
                    session.hasGeneratedTraffic() &&
                    session.isComplete() &&
                    !session.isCloseRequest()
                ) {
                    session.setCloseRequest(true);

                    engine.schedule(
                        std::make_unique<TCPConnectionCloseEvent>(
                            engine.now(),
                            session.getSession_id()
                        )
                    );
                }

                break;
            }

            case PacketType::DATA:
            {
                const auto expected_before =
                    receiver.getExpectedAckNum();

                if (
                    !receiver.receive_data(
                        packet.tcp.seq,
                        packet.tcp.payload,
                        engine.now()
                    )
                ) {
                    break;
                }

                const auto expected_after =
                    receiver.getExpectedAckNum();

                /*
                * Out-of-order data must be ACKed immediately.
                */
                if (
                    packet.tcp.seq != expected_before
                ) {
                    Packet ack(
                        receiver.getLocalNode(),
                        receiver.getRemoteNode(),
                        receiver.getLocalNode(),
                        engine.now(),
                        engine.getGlobalPacketSize(),
                        packet.session_id
                    );

                    ack.packet_type =
                        PacketType::ACK;

                    ack.tcp =
                        receiver.buildAck();

                    ack.departure_time =
                        engine.now();

                    receiver.clearDelayedAckPending();

                    PacketUtils::sendPacketThroughTopology(
                        engine,
                        ack
                    );

                    break;
                }

                /*
                * A second in-order segment arriving while an ACK
                * is pending causes an immediate ACK.
                */
                if (
                    receiver.hasDelayedAckPending()
                ) {
                    Packet ack(
                        receiver.getLocalNode(),
                        receiver.getRemoteNode(),
                        receiver.getLocalNode(),
                        engine.now(),
                        engine.getGlobalPacketSize(),
                        packet.session_id
                    );

                    ack.packet_type =
                        PacketType::ACK;

                    ack.tcp =
                        receiver.buildAck();

                    ack.departure_time =
                        engine.now();

                    receiver.clearDelayedAckPending();

                    PacketUtils::sendPacketThroughTopology(
                        engine,
                        ack
                    );

                    break;
                }

                /*
                * First in-order segment:
                * delay the ACK by 200 ms.
                */
                receiver.markDelayedAckPending();

                engine.schedule(
                    std::make_unique<TCPDelayedAckEvent>(
                        engine.now() +
                            TCPDelayedAckEvent::DEFAULT_DELAY,
                        session.getSession_id(),
                        receiver.getLocalNode(),
                        expected_after
                    )
                );

                break;
            }

            case PacketType::RST: {
                receiver.failRetransmission();

                if (session.getState() == TCPState::CLOSED) {
                    engine.releaseTCPListenerSession(session.getSession_id());
                }
                break;
            }

            case PacketType::FIN: {
                if (!receiver.receive_fin(packet.tcp.seq)) {
                    break;
                }

                if (client.getTcpState() == TCPState::TIME_WAIT) {
                    engine.schedule(std::make_unique<TCPTimeWaitTimeoutEvent>(engine.now() + 0.1, session.getSession_id()));
                }

                Packet ack(
                    receiver.getLocalNode(),
                    receiver.getRemoteNode(),
                    receiver.getLocalNode(),
                    engine.now(),
                    engine.getGlobalPacketSize(),
                    packet.session_id
                );

                ack.tcp = receiver.buildAck();
                ack.packet_type = inferPacketType(ack.tcp);

                PacketUtils::sendPacketThroughTopology(engine, ack);

                if (receiver.getTcpState() == TCPState::CLOSE_WAIT) {
                    if (receiver.send_fin()) {
                        Packet fin(
                            receiver.getLocalNode(),
                            receiver.getRemoteNode(),
                            receiver.getLocalNode(),
                            engine.now(),
                            engine.getGlobalPacketSize(),
                            packet.session_id
                        );

                        fin.tcp = receiver.buildFin();
                        fin.packet_type = inferPacketType(fin.tcp);

                        PacketUtils::sendPacketThroughTopology(engine, fin);
                    }
                }

                if (session.getState() == TCPState::CLOSED) {
                    engine.releaseTCPListenerSession(session.getSession_id());
                }
                break;
            }

            default:
                break;
        }
    }

}
