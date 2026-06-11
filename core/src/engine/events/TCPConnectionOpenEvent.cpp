#include "core/events/TCPConnectionOpenEvent.hpp"

namespace kns {

    TCPConnectionOpenEvent::TCPConnectionOpenEvent(
        double timestamp,
        std::uint64_t session_id
    )
    : Event(timestamp),
      session_id(session_id)
    {
    }

    void TCPConnectionOpenEvent::execute(
        SimulationEngine& engine
    )
    {
    }

}