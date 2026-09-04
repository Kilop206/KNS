#include "engine/events/TCPTimeoutEvent.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/transport/tcp/TCPSession.hpp"

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

        /*
        * Logical timer cancellation:
        *
        * If the segment disappeared from the send buffer,
        * it was cumulatively acknowledged before this timeout
        * event reached the queue.
        */
        if (!client.hasOutstandingSegment(sequence_)) {
            return;
        }

        KNS_DEBUG_LOG(
            "[TCP][RTO] timeout "
            << "session=" << session_id_
            << " seq=" << sequence_
            << " time=" << engine.now()
            << '\n'
        );

        client.onSendTimeout();
    }

} // namespace kns