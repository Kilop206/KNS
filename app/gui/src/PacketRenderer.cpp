#include "../include/PacketRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/core/SimulationEngine.hpp"
#include "network/Topology.hpp"

namespace interface {

namespace {
    ImU32 makePacketColor(double progress, double pulse) {
        const int r = static_cast<int>(150.0 + 80.0 * pulse);
        const int g = static_cast<int>(80.0 + 60.0 * (1.0 - progress));
        const int b = static_cast<int>(200.0 + 40.0 * progress);
        return IM_COL32(
            std::clamp(r, 0, 255),
            std::clamp(g, 0, 255),
            std::clamp(b, 0, 255),
            255
        );
    }
}

void PacketRenderer::render(
    ImDrawList* draw_list,
    const kns::Topology& topo,
    const std::vector<std::pair<float, float>>& positions,
    kns::SimulationEngine& engine,
    double minimum_visible_duration_seconds
) const {
    (void)topo;

    const auto& packets = engine.getPacketsInTransit();
    const double now = engine.now();

    for (const auto& packet : packets) {
        if (packet.from_node < 0 || packet.to_node < 0 ||
            packet.from_node >= static_cast<int>(positions.size()) ||
            packet.to_node >= static_cast<int>(positions.size())) {
            continue;
        }

        const double actual_duration = packet.arrival_time - packet.departure_time;
        if (actual_duration <= 0.0) {
            continue;
        }

        const double visual_duration = std::max(actual_duration, minimum_visible_duration_seconds);
        double t = (now - packet.departure_time) / visual_duration;
        t = std::clamp(t, 0.0, 1.0);

        const float x = static_cast<float>(
            positions[packet.from_node].first +
            (positions[packet.to_node].first - positions[packet.from_node].first) * t
        );

        const float y = static_cast<float>(
            positions[packet.from_node].second +
            (positions[packet.to_node].second - positions[packet.from_node].second) * t
        );

        const float px = positions[packet.from_node].first;
        const float py = positions[packet.from_node].second;
        const float qx = positions[packet.to_node].first;
        const float qy = positions[packet.to_node].second;

        const double pulse = 0.5 + 0.5 * std::sin(now * 10.0 + packet.departure_time * 7.0);
        const ImU32 line_color = IM_COL32(180, 120, 220, 90);
        const ImU32 packet_color = makePacketColor(t, pulse);

        draw_list->AddLine(ImVec2(px, py), ImVec2(qx, qy), line_color, 1.5f);
        draw_list->AddCircleFilled(ImVec2(x, y), 6.0f + static_cast<float>(pulse * 2.0), packet_color);
        draw_list->AddCircle(ImVec2(x, y), 8.5f, IM_COL32(255, 255, 255, 160), 0, 1.2f);
    }
}

} // namespace interface
