#include "../include/MetricsPannel.hpp"

#include <cfloat>

namespace interface {
    void MetricsPannel::render(const kns::Stats& stats, const CircularBuffer& buffer) {
        const float min_latency = buffer.getMinimumLatency();
        const float avg_latency = buffer.getAverageLatency();
        const float max_latency = buffer.getMaximumLatency();

        ImGui::Text(
            "Packets Sent: %d\n"
            "Packets Delivered: %d\n"
            "Packets Lost: %d",
            stats.packets_sent,
            stats.packets_delivered,
            stats.packets_lost
        );

        ImGui::Separator();

        if (buffer.empty()) {
            ImGui::Text("Latency samples: 0");
            ImGui::Text("Waiting for delivered packets...");
            return;
        }

        const std::vector<float> samples = buffer.values();

        ImGui::Text("Latency samples: %zu", samples.size());
        ImGui::PlotLines(
            "Latency (s)",
            samples.data(),
            static_cast<int>(samples.size()),
            0,
            nullptr,
            FLT_MAX,
            FLT_MAX,
            ImVec2(0, 120)
        );

        ImGui::Text("Minimum Latency: %.6f", min_latency);
        ImGui::Text("Average Latency: %.6f", avg_latency);
        ImGui::Text("Maximum Latency: %.6f", max_latency);
    }
}
