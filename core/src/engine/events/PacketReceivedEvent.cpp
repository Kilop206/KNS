#include "engine/events/PacketReceivedEvent.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"
#include "network/transport/tcp/TCPSession.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns {

    PacketReceivedEvent::PacketReceivedEvent(double timestamp, Packet packet)
        : Event(timestamp), packet(std::move(packet)) {}

    void PacketReceivedEvent::execute(SimulationEngine& engine) {
        std::cout
            << "[RECEIVED] type="
            << static_cast<int>(packet.packet_type)
            << " session="
            << packet.session_id
            << " node="
            << packet.current_node
            << '\n';

        assert(packet.current_node >= 0);

        if (packet.current_node != packet.destination) {
            const int next = engine.getNextHop(packet.current_node, packet.destination);

            if (next == -1) {
                engine.getStats().packets_lost++;

                std::cout
                    << "[DROPPED] Packet from "
                    << packet.source
                    << " to "
                    << packet.destination
                    << " at time "
                    << engine.now()
                    << '\n';

                engine.removePacketInTransit(packet.departure_time, timestamp_);
                return;
            }

            const auto& links = engine.getTopology().getLinksFromNode(packet.current_node);

            const Link* selected_link = nullptr;
            for (const Link& link : links) {
                if (link.getOtherNode(packet.current_node) == next) {
                    selected_link = &link;
                    break;
                }
            }

            if (!selected_link) {
                engine.getStats().packets_lost++;

                std::cout
                    << "[DROPPED] Packet from "
                    << packet.source
                    << " to "
                    << packet.destination
                    << " at time "
                    << engine.now()
                    << '\n';

                engine.removePacketInTransit(packet.departure_time, timestamp_);
                return;
            }

            engine.removePacketInTransit(packet.departure_time, timestamp_);
            engine.sendPacket(packet, *selected_link, timestamp_);
            return;
        }

        auto& stats = engine.getStats();
        stats.packets_delivered++;

        const double latency = engine.now() - packet.creation_time;
        stats.total_latency += latency;

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6)
            << "[DELIVERED] Packet from "
            << packet.source
            << " to "
            << packet.destination
            << " latency="
            << latency;
        std::cout << oss.str() << '\n';

        std::cout << "[LATENCY] " << std::fixed << std::setprecision(6)
                << latency << '\n';

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

                server.receive_syn(packet.seq_num);

                Packet synAck(
                    server.getLocalNode(),
                    server.getRemoteNode(),
                    server.getLocalNode(),
                    engine.now(),
                    engine.getGlobalPacketSize(),
                    packet.session_id
                );

                synAck.packet_type = PacketType::SYN_ACK;
                synAck.seq_num = server.getSeqNum();
                synAck.ack_num = server.getExpectedAckNum();

                PacketUtils::sendPacketThroughTopology(engine, synAck);
                break;
            }

            case PacketType::SYN_ACK: {
                std::cout
                    << "[RECEIVED_SYN_ACK] session="
                    << packet.session_id
                    << '\n';

                client.receive_syn_ack(packet.seq_num, packet.ack_num);

                Packet ack(
                    client.getLocalNode(),
                    client.getRemoteNode(),
                    client.getLocalNode(),
                    engine.now(),
                    engine.getGlobalPacketSize(),
                    packet.session_id
                );

                ack.packet_type = PacketType::ACK;
                ack.seq_num = client.getSeqNum();
                ack.ack_num = client.send_ack();

                PacketUtils::sendPacketThroughTopology(engine, ack);
                break;
            }

            case PacketType::ACK: {
                std::cout
                    << "[RECEIVED_ACK] session="
                    << packet.session_id
                    << '\n';

                server.receive_ack(packet.ack_num);

                if (
                    client.getTcpState() == TCPState::ESTABLISHED &&
                    server.getTcpState() == TCPState::ESTABLISHED &&
                    session.getState() != TCPState::ESTABLISHED
                ) {
                    session.setState(TCPState::ESTABLISHED);

                    std::cout
                        << "[TCP][SESSION "
                        << session.getSession_id()
                        << "] SESSION_ESTABLISHED\n";

                    engine.generatePackets(engine.now(), session);
                }
                break;
            }

            case PacketType::DATA: {
                std::cout
                    << "[DATA] session="
                    << packet.session_id
                    << " delivered"
                    << '\n';
                break;
            }

            default:
                break;
        }
    }
}