#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"
#include "network/Packet.hpp"

namespace kns {

    class TCPSession;

    class PacketReceivedEvent : public Event {
        public:
            explicit PacketReceivedEvent(double timestamp, Packet packet);

            void execute(SimulationEngine& engine) override;
            const char* getName() const noexcept override { return "PacketReceivedEvent"; }

        private:
            Packet packet;
    };

}
