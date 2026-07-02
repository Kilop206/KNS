#pragma once

#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"

namespace kns {

    class PacketUtils {
    public:
        static bool sendPacketThroughTopology(
            SimulationEngine& engine,
            const Packet& pkt
        );
    };

}