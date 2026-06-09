#pragma once

#include <cstdint>

#include "network/Packet.hpp"
#include "engine/core/Event.hpp"

namespace kns {

    class PacketReceivedEvent : public Event {
    public:
        Packet packet;

        PacketReceivedEvent(double timestamp, Packet packet);

        void execute(SimulationEngine& engine) override;

        void execute(SimulationEngine& engine, uint64_t session_id);
    };

}