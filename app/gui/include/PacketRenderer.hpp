#pragma once

#include <utility>
#include <vector>

#include "imgui.h"

namespace kns {
    class SimulationEngine;
    class Topology;
}

namespace interface {

class PacketRenderer {
public:
    void render(
        ImDrawList* draw_list,
        const kns::Topology& topo,
        const std::vector<std::pair<float, float>>& positions,
        kns::SimulationEngine& engine,
        double minimum_visible_duration_seconds = 0.35
    ) const;
};

} // namespace interface
