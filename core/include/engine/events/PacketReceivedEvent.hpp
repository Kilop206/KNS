#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"
#include "network/Packet.hpp"

namespace kns {

    class PacketReceivedEvent : public Event {
        public:
            explicit PacketReceivedEvent(double timestamp, Packet packet);

            void execute(SimulationEngine& engine) override;

        private:
            Packet packet;
    };

}