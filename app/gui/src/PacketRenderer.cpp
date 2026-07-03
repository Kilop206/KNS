#include "../include/PacketRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstdint>

namespace gui {
    ImU32 PacketRenderer::packetColorByType(kns::PacketType type) {
        switch (type) {
            case kns::PacketType::SYN:     return IM_COL32(255, 200,  40, 255); // yellow
            case kns::PacketType::SYN_ACK: return IM_COL32(180, 120, 255, 255); // purple
            case kns::PacketType::ACK:     return IM_COL32( 60, 190, 255, 255); // blue
            case kns::PacketType::DATA:    return IM_COL32( 80, 220, 120, 255); // green
            case kns::PacketType::FIN:     return IM_COL32(255, 120,  60, 255); // orange
            default:                       return IM_COL32(200, 200, 200, 255); // gray
        }
    }

    ImU32 PacketRenderer::packetBorderColor(kns::PacketType type) {
        switch (type) {
            case kns::PacketType::SYN:
            case kns::PacketType::SYN_ACK:
                return IM_COL32(255, 255, 255, 200);
            case kns::PacketType::ACK:
                return IM_COL32(255, 255, 255, 170);
            case kns::PacketType::DATA:
                return IM_COL32(255, 255, 255, 160);
            case kns::PacketType::FIN:
                return IM_COL32(255, 255, 255, 180);
            default:
                return IM_COL32(255, 255, 255, 150);
        }
    }

    void PacketRenderer::render(
        ImDrawList* draw_list,
        const std::vector<std::pair<float, float>>& positions,
        const std::vector<VisualPacket>& packets,
        double visual_time
    ) const
    {
        for (const auto& packet : packets) {

            if (visual_time < packet.visual_start_time) {
                continue;
            }
            
            std::cout
                << "FROM "
                << packet.from
                << " TO "
                << packet.to
                << "  x="
                << positions[packet.from].first
                << " y="
                << positions[packet.from].second
                << " -> "
                << positions[packet.to].first
                << ","
                << positions[packet.to].second
                << '\n';

            const double actual_duration = packet.sim_arrival_time - packet.sim_departure_time;
            if (actual_duration <= 0.0) {
                continue;
            }
            
            const double visual_duration = std::max(packet.visual_duration, 0.001);
            const double elapsed = visual_time - packet.visual_start_time;

            if (elapsed < 0.0) {
                continue;
            }

            double t = elapsed / visual_duration;
            t = (visual_time - packet.visual_start_time) / actual_duration;

            const float x = static_cast<float>(
                positions[packet.from].first +
                (positions[packet.to].first - positions[packet.from].first) * t
            );

            const float y = static_cast<float>(
                positions[packet.from].second +
                (positions[packet.to].second - positions[packet.from].second) * t
            );

            const float pulse = static_cast<float>(0.5 + 0.5 * std::sin(visual_time * 10.0 + packet.visual_start_time * 7.0));

            const ImU32 fill_color = packetColorByType(packet.type);
            const ImU32 border_color = packetBorderColor(packet.type);

            const float base_radius = 10.0f;
            const float radius = base_radius + pulse * 3.0f;

            std::cout
                << "t=" << t
                << " x=" << x
                << " y=" << y
                << '\n';

                draw_list->AddCircleFilled(
                    ImVec2(100,100),
                    20,
                    IM_COL32(255,0,0,255)
                );

            draw_list->AddCircleFilled(ImVec2(x, y), radius, fill_color);
            draw_list->AddCircle(ImVec2(x, y), radius + 2.5f, border_color, 0, 1.2f);
        }
    }
}