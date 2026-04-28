#pragma once

#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"

namespace kns {
    bool sendPacketThroughTopology(SimulationEngine& engine, Packet pkt);
}