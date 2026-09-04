#include "engine/events/TCPTimeoutEvent.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "engine/events/TCPRetransmissionEvent.hpp"
#include "network/transport/tcp/TCPSession.hpp"

#include <memory>

namespace kns {

    TCPTimeoutEvent::TCPTimeoutEvent(
        double timestamp,
        std::uint64_t session_id,
        std::uint32_t sequence
    )
        : Event(timestamp),
          session_id_(session_id),
          sequence_(sequence)
    {
    }

    void TCPTimeoutEvent::execute(
        SimulationEngine& engine
    )
    {
        if (!engine.hasTCPSession(session_id_)) {
            return;
        }

        auto& session =
            engine.getTCPSession(session_id_);

        auto& client =
            session.getClientConnection();

        const auto segment =
            client.getOutstandingSegment(sequence_);

        if (!segment.has_value()) {
            return;
        }

        const auto oldest =
            client.getOldestOutstandingSequence();

        if (
            !oldest.has_value() ||
            *oldest != sequence_
        ) {
            return;
        }

        client.onSendTimeout();

        if (!client.isEstablished()) {
            return;
        }

        engine.schedule(
            std::make_unique<TCPRetransmissionEvent>(
                engine.now(),
                session_id_,
                sequence_
            )
        );
    }

} // namespace kns