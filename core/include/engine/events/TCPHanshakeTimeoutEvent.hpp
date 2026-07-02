#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"

namespace kns {

    class TCPHandshakeTimeoutEvent : public Event {
        public:
            TCPHandshakeTimeoutEvent(double timestamp, std::uint64_t session_id);

            void execute(SimulationEngine& engine) override;

        private:
            std::uint64_t session_id_;
    };

}