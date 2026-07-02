#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"

namespace kns {

    class PacketGenerationEvent : public Event {
        public:
            PacketGenerationEvent(
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