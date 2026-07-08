#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"
#include "network/Packet.hpp"
#include "network/transport/tcp/TCPSession.hpp"

namespace kns {

    class PacketReceivedEvent : public Event {
        public:
            explicit PacketReceivedEvent(double timestamp, Packet packet);

            static void refreshSessionState(kns::TCPSession& session);

            void execute(SimulationEngine& engine) override;

        private:
            Packet packet;
    };

}