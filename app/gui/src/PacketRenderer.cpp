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
        const VisualPacket* hovered_packet = nullptr;
        float hovered_dist2 = 0.0f;
        double hovered_t = 0.0;

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

            const std::size_t node_count = positions.size();
            if (packet.from < 0 || packet.to < 0 ||
                static_cast<std::size_t>(packet.from) >= node_count ||
                static_cast<std::size_t>(packet.to) >= node_count) {
                continue;
            }

            const double t = std::clamp(elapsed / visual_duration, 0.0, 1.0);

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

            draw_list->AddCircleFilled(ImVec2(x, y), radius, fill_color);
            draw_list->AddCircle(ImVec2(x, y), radius + 2.5f, border_color, 0, 1.2f);

            const ImVec2 mouse = ImGui::GetMousePos();

            const float dx = mouse.x - x;
            const float dy = mouse.y - y;
            const float dist2 = dx * dx + dy * dy;

            if (dist2 <= radius * radius &&
                (hovered_packet == nullptr || dist2 < hovered_dist2)) {
                hovered_packet = &packet;
                hovered_dist2 = dist2;
                hovered_t = t;
            }
        }

        if (hovered_packet != nullptr) {
            const float progress = static_cast<float>(hovered_t * 100.0);

            ImGui::BeginTooltip();

            ImGui::Text("Type: %s",
                packetTypeToString(hovered_packet->type));

            ImGui::Text("Session: %llu",
                static_cast<unsigned long long>(hovered_packet->session_id));

            ImGui::Text("From: %d", hovered_packet->from);
            ImGui::Text("To: %d", hovered_packet->to);

            ImGui::Text("Progress: %.0f%%", progress);

            ImGui::Text("Departure: %.3f",
                hovered_packet->sim_departure_time);

            ImGui::Text("Arrival: %.3f",
                hovered_packet->sim_arrival_time);

            ImGui::EndTooltip();
        }
    }
}