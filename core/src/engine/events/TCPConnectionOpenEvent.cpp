#include "engine/events/TCPConnectionOpenEvent.hpp"

#include "engine/core/SimulationEngine.hpp"

namespace kns {

    void TCPConnectionOpenEvent::execute(SimulationEngine&) {
        // Deprecated: handshake now happens directly in PacketReceivedEvent.
    }

}