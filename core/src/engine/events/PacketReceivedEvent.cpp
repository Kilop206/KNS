#include "engine/events/PacketReceivedEvent.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "engine/CORE/SimulationEngine.hpp"
#include "engine/events/TCPConnectionOpenEvent.hpp"
#include "network/transport/tcp/TCPSession.hpp"

namespace kns
{

    PacketReceivedEvent::PacketReceivedEvent(
        double timestamp,
        Packet packet
    )
        : Event(timestamp),
        packet(std::move(packet))
    {
    }

    void PacketReceivedEvent::execute(SimulationEngine& engine)
    {
        execute(engine, packet.session_id);
    }

    void PacketReceivedEvent::execute(
        SimulationEngine& engine,
        uint64_t session_id
    )
    {
        std::cout
            << "[RECEIVED] type="
            << static_cast<int>(packet.packet_type)
            << " session="
            << packet.session_id
            << " node="
            << packet.current_node
            << '\n';

        if (packet.current_node != packet.destination)
        {
            int next =
                engine.getNextHop(
                    packet.current_node,
                    packet.destination
                );

            if (next == -1)
            {
                engine.getStats().packets_lost++;

                engine.removePacketInTransit(
                    packet.departure_time,
                    timestamp_
                );

                return;
            }

            const auto& links =
                engine.getTopology().getLinksFromNode(
                    packet.current_node
                );

            for (const Link& link : links)
            {
                if (link.getOtherNode(packet.current_node) == next)
                {
                    engine.removePacketInTransit(
                        packet.departure_time,
                        timestamp_
                    );

                    engine.sendPacket(
                        packet,
                        link,
                        timestamp_
                    );

                    return;
                }
            }

            engine.getStats().packets_lost++;

            engine.removePacketInTransit(
                packet.departure_time,
                timestamp_
            );

            return;
        }

        auto& stats = engine.getStats();

        stats.packets_delivered++;

        double latency =
            engine.now() - packet.creation_time;

        stats.total_latency += latency;

        engine.notifyLatencyDelivered(latency);

        engine.removePacketInTransit(
            packet.departure_time,
            timestamp_
        );

        std::cout
            << "[DELIVERED] "
            << packet.source
            << " -> "
            << packet.destination
            << " latency="
            << latency
            << '\n';

        auto& session =
            engine.getTCPSession(
                packet.session_id
            );

        auto& client =
            session.getClientConnection();

        auto& server =
            session.getServerConnection();

        switch (packet.packet_type)
        {

        case PacketType::SYN:
        {
            server.receive_syn(
                packet.seq_num
            );

            engine.schedule(
                std::make_unique<TCPConnectionOpenEvent>(
                    engine.now(),
                    packet.session_id
                )
            );

            break;
        }

        case PacketType::SYN_ACK:
        {
            client.receive_syn_ack(
                packet.seq_num,
                packet.ack_num
            );

            engine.schedule(
                std::make_unique<TCPConnectionOpenEvent>(
                    engine.now(),
                    packet.session_id
                )
            );

            break;
        }

        case PacketType::ACK:
        {
            server.receive_ack(
                packet.ack_num
            );

            if (
                client.getTcpState() == TCPState::ESTABLISHED &&
                server.getTcpState() == TCPState::ESTABLISHED &&
                session.getState() != TCPState::ESTABLISHED
            )
            {
                session.setState(
                    TCPState::ESTABLISHED
                );

                std::cout
                    << "[TCP][SESSION "
                    << session.getSession_id()
                    << "] ESTABLISHED\n";

                engine.generatePackets(
                    engine.now(),
                    session
                );
            }

            break;
        }

        case PacketType::DATA:
        {
            std::cout
                << "[DATA] delivered session="
                << packet.session_id
                << '\n';

            break;
        }

        default:
            break;
        }
    }
}