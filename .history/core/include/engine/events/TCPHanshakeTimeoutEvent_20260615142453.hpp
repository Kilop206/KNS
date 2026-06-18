#pragma once

#include "engine/core/Event.hpp"
#include "engine/core/SimulationEngine.hpp"

#include <cstdint>

namespace kns {

    class TCPHandshakeTimeoutEvent : public Event {
    private:
        std::uint64_t session_id;

    public:
        TCPHandshakeTimeoutEvent(double timestamp, std::uint64_t session_id);

        void execute(SimulationEngine& engine) override;
    };
}