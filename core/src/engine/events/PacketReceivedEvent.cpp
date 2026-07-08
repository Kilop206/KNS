#include "engine/events/PacketReceivedEvent.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <memory>

#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPConnectionCloseEvent.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns {

    PacketReceivedEvent::PacketReceivedEvent(double timestamp, Packet packet)
        : Event(timestamp),
          packet(std::move(packet))
    {
    }

    void PacketReceivedEvent::refreshSessionState(kns::TCPSession& session)
    {
        const auto clientState = session.getClientConnection().getTcpState();
        const auto serverState = session.getServerConnection().getTcpState();

        if (clientState == kns::TCPState::ESTABLISHED &&
            serverState == kns::TCPState::ESTABLISHED)
        {
            session.setState(kns::TCPState::ESTABLISHED);
        }
        else if (clientState == kns::TCPState::CLOSE_WAIT ||
                serverState == kns::TCPState::CLOSE_WAIT)
        {
            session.setState(kns::TCPState::CLOSE_WAIT);
        }
        else if (clientState == kns::TCPState::LAST_ACK ||
                serverState == kns::TCPState::LAST_ACK)
        {
            session.setState(kns::TCPState::LAST_ACK);
        }
        else if (clientState == kns::TCPState::TIME_WAIT ||
                serverState == kns::TCPState::TIME_WAIT)
        {
            session.setState(kns::TCPState::TIME_WAIT);
        }
        else if (clientState == kns::TCPState::CLOSED &&
                serverState == kns::TCPState::CLOSED)
        {
            session.setState(kns::TCPState::CLOSED);
        }
    }

    void PacketReceivedEvent::execute(SimulationEngine& engine)
    {
        packet.packet_type = inferPacketType(packet.tcp);

        assert(packet.current_node >= 0);

        if (packet.current_node != packet.destination) {
            auto& stats = engine.getStats();

            if (!PacketUtils::sendPacketThroughTopology(engine, packet)) {
                stats.packets_lost++;
                std::cout
                    << "[DROPPED] Packet from "
                    << packet.source
                    << " to "
                    << packet.destination
                    << " at time "
                    << engine.now()
                    << '\n';
            }

            engine.removePacketInTransit(packet.departure_time, timestamp_);
            return;
        }

        auto& stats = engine.getStats();
        stats.packets_delivered++;

        const double latency = engine.now() - packet.creation_time;
        stats.total_latency += latency;

        engine.notifyLatencyDelivered(latency);
        engine.removePacketInTransit(packet.departure_time, timestamp_);

        auto& session = engine.getTCPSession(packet.session_id);
        auto& client = session.getClientConnection();
        auto& server = session.getServerConnection();

        switch (packet.packet_type) {
            case PacketType::SYN: {
                std::cout
                    << "[RECEIVED_SYN] session="
                    << packet.session_id
                    << '\n';

                server.receive_syn(packet.tcp.seq);

                Packet synAck(
                    server.getLocalNode(),
                    server.getRemoteNode(),
                    server.getLocalNode(),
                    engine.now(),
                    engine.getGlobalPacketSize(),
                    packet.session_id
                );

                synAck.tcp = server.buildSynAck();
                synAck.packet_type = inferPacketType(synAck.tcp);

                PacketUtils::sendPacketThroughTopology(engine, synAck);
                break;
            }

            case PacketType::SYN_ACK: {
                std::cout
                    << "[RECEIVED_SYN_ACK] session="
                    << packet.session_id
                    << '\n';

                client.receive_syn_ack(packet.tcp.seq, packet.tcp.ack);

                refreshSessionState(session);

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

            case PacketType::ACK: {
                std::cout << "[RECEIVED_ACK] session=" << packet.session_id << '\n';

                server.receive_ack(packet.tcp.ack);

                if (
                    client.getTcpState() == TCPState::ESTABLISHED &&
                    server.getTcpState() == TCPState::ESTABLISHED &&
                    !session.hasGeneratedTraffic()
                ) {
                    session.setState(TCPState::ESTABLISHED);
                    engine.generatePackets(engine.now(), session);
                }

                refreshSessionState(session);
                break;
            }

            case PacketType::FIN: {
                std::cout << "[RECEIVED_FIN] session=" << packet.session_id << '\n';

                server.receive_fin(packet.tcp.seq);
                session.setState(TCPState::CLOSE_WAIT);

                Packet ack(
                    server.getLocalNode(),
                    server.getRemoteNode(),
                    server.getLocalNode(),
                    engine.now(),
                    engine.getGlobalPacketSize(),
                    packet.session_id
                );

                ack.tcp = server.buildAck();
                ack.packet_type = inferPacketType(ack.tcp);

                PacketUtils::sendPacketThroughTopology(engine, ack);
                break;
            }

            case PacketType::DATA: {
                break;
            }

            default:
                break;
        }
    }
}