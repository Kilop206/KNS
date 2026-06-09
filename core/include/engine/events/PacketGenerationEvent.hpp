#pragma once

#include "engine/core/Event.hpp"
#include "enums/PacketType.hpp"
#include "engine/core/SimulationEngine.hpp"

namespace kns {

class PacketGenerationEvent : public Event {
    private:
        int source_;
        int destination_;
        uint64_t session_id_;

    public:
        PacketGenerationEvent(
            double timestamp,
            int source,
            int destination,
            uint64_t session_id
        );

        void execute(SimulationEngine& engine) override;
    };

}