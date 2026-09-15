#include "engine/events/TCPFinRetryEvent.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/utils/PacketUtils.hpp"

namespace kns {
    void TCPFinRetryEvent::execute(SimulationEngine& engine) {
        if (!engine.hasTCPSession(session_id_)) return;
        auto& session = engine.getTCPSession(session_id_);
        auto& endpoint = node_ == session.getSource()
            ? session.getClientConnection() : session.getServerConnection();
        const auto state = endpoint.getTcpState();
        if (state != TCPState::FIN_WAIT_1 && state != TCPState::LAST_ACK &&
            state != TCPState::CLOSING) return;
        if (retries_ == MAX_RETRIES) {
            session.failClose();
            engine.releaseTCPListenerSession(session_id_);
            return;
        }
        Packet fin(endpoint.getLocalNode(), endpoint.getRemoteNode(), endpoint.getLocalNode(),
                   engine.now(), Packet::TCP_IPV4_HEADER_BYTES, session_id_);
        fin.tcp = endpoint.buildFin();
        fin.packet_type = PacketType::FIN;
        PacketUtils::sendPacketThroughTopology(engine, fin);
        engine.schedule(std::make_unique<TCPFinRetryEvent>(
            engine.now() + INTERVAL, session_id_, node_, retries_ + 1));
    }
}
