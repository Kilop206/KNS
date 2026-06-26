#pragma once

#include <utility>
#include <vector>
#include "imgui.h"

#include "../include/VisualPacket.hpp"
#include "enums/PacketType.hpp"

namespace kns {
    class SimulationEngine;
    class Topology;
}

namespace interface {

    class PacketRenderer {
        public:
            void render(
                ImDrawList* draw_list,
                const std::vector<std::pair<float, float>>& positions,
                const std::vector<VisualPacket>& packets,
                double visual_time
            ) const;

        private:
            static ImU32 packetColorByType(kns::PacketType type);
            static ImU32 packetBorderColor(kns::PacketType type);
        };
}