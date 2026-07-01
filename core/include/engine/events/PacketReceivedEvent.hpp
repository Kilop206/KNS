#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"
#include "network/Packet.hpp"

namespace kns {

    class SimulationEngine;

    class PacketReceivedEvent : public Event {
    public:
        Packet packet;

        PacketReceivedEvent(double timestamp, Packet packet);

        void execute(SimulationEngine& engine) override;
    };

}