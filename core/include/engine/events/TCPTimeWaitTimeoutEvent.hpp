#pragma once

#include "engine/core/Event.hpp"
#include <cstdint>

namespace kns {
    class SimulationEngine;

    class TCPTimeWaitTimeoutEvent : public Event {
        public:
            TCPTimeWaitTimeoutEvent(double timestamp, std::uint64_t session_id);

            void execute(SimulationEngine& engine) override;

        private:
            std::uint64_t session_id_;
    };
}