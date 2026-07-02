#pragma once

#include "engine/core/Event.hpp"

#include <cstdint>

namespace kns {

    class TCPHandshakeEvent : public Event {
        public:
            TCPHandshakeEvent(
                double timestamp,
                int source,
                int destination,
                std::uint64_t session_id
            );

            void execute(SimulationEngine& engine) override;

        private:
            int source_;
            int destination_;
            std::uint64_t session_id_;
    };

}