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

    const char* PacketRenderer::packetTypeToString(kns::PacketType type)
    {
        switch (type) {
            case kns::PacketType::SYN:     return "SYN";
            case kns::PacketType::SYN_ACK: return "SYN-ACK";
            case kns::PacketType::ACK:     return "ACK";
            case kns::PacketType::DATA:    return "DATA";
            case kns::PacketType::FIN:     return "FIN";
            default:                       return "UNKNOWN";
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

                draw_list->AddCircleFilled(
                    ImVec2(100,100),
                    20,
                    IM_COL32(255,0,0,255)
                );

            draw_list->AddCircleFilled(ImVec2(x, y), radius, fill_color);
            draw_list->AddCircle(ImVec2(x, y), radius + 2.5f, border_color, 0, 1.2f);

            const ImVec2 mouse = ImGui::GetMousePos();

            const float dx = mouse.x - x;
            const float dy = mouse.y - y;
            const float dist2 = dx * dx + dy * dy;

            if (dist2 <= radius * radius) {

                const float progress =
                    static_cast<float>(
                        ((visual_time - packet.visual_start_time) /
                        packet.visual_duration) * 100.0
                    );

                ImGui::BeginTooltip();

                ImGui::Text("Type: %s",
                    packetTypeToString(packet.type));

                ImGui::Text("Session: %llu",
                    static_cast<unsigned long long>(packet.session_id));

                ImGui::Text("From: %d", packet.from);
                ImGui::Text("To: %d", packet.to);

                ImGui::Text("Progress: %.0f%%",
                    std::clamp(progress, 0.0f, 100.0f));

                ImGui::Text("Departure: %.3f",
                    packet.sim_departure_time);

                ImGui::Text("Arrival: %.3f",
                    packet.sim_arrival_time);

                ImGui::EndTooltip();
            }
        }
    }
}