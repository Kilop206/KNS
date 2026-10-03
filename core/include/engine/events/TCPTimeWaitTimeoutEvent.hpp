#pragma once

#include "engine/core/Event.hpp"
#include <cstdint>

namespace kns {
    class SimulationEngine;

    class TCPTimeWaitTimeoutEvent : public Event {
        public:
            TCPTimeWaitTimeoutEvent(double timestamp, std::uint64_t session_id);

            void execute(SimulationEngine& engine) override;
            const char* getName() const noexcept override { return "TCPTimeWaitTimeoutEvent"; }

        private:
            std::uint64_t session_id_;
    };
}
