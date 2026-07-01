#pragma once

#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"

namespace kns {
    class PacketUtils {
        public:
            bool static sendPacketThroughTopology(SimulationEngine& engine, Packet& pkt);
    };
}