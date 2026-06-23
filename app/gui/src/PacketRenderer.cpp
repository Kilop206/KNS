#include "../include/PacketRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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
        const std::vector<std::pair<float, float>>& positions,
        const std::vector<VisualPacket>& packets,
        double visual_time
    ) const {

        for (const auto& packet : packets) {
            if (packet.from < 0 || packet.to < 0 ||
                packet.from >= static_cast<int>(positions.size()) ||
                packet.to >= static_cast<int>(positions.size())) {
                continue;
            }

            const double actual_duration = packet.sim_arrival_time - packet.sim_departure_time;
            if (actual_duration <= 0.0) {
                continue;
            }

            const double visual_duration = packet.visual_duration;

            double t =
                (visual_time - packet.visual_start_time)
                / packet.visual_duration;

            t = std::clamp(t, 0.0, 1.0);

            const float x = static_cast<float>(
                positions[packet.from].first +
                (positions[packet.to].first - positions[packet.from].first) * t
            );

            const float y = static_cast<float>(
                positions[packet.from].second +
                (positions[packet.to].second - positions[packet.from].second) * t
            );

            const float px = positions[packet.from].first;
            const float py = positions[packet.from].second;
            const float qx = positions[packet.to].first;
            const float qy = positions[packet.to].second;

            const double pulse = 0.5 + 0.5 * std::sin(visual_time * 10.0 + packet.visual_start_time * 7.0);
            const ImU32 line_color = IM_COL32(180, 120, 220, 90);
            const ImU32 packet_color = makePacketColor(t, pulse);
            
            draw_list->AddCircleFilled(
                ImVec2(x, y),
                6.0f + static_cast<float>(pulse * 2.0),
                packet_color
            );
            draw_list->AddCircle(ImVec2(x, y), 8.5f, IM_COL32(255, 255, 255, 160), 0, 1.2f);
        }
    }

}