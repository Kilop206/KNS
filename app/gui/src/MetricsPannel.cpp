#include "../include/MetricsPannel.hpp"
#include "../include/TranslationService.hpp"

#include <cfloat>

namespace gui {
    void MetricsPannel::render(
        const kns::Stats& stats,
        const CircularBuffer& buffer,
        TranslationService& translations
    ) {
        const float min_latency = buffer.getMinimumLatency();
        const float avg_latency = buffer.getAverageLatency();
        const float max_latency = buffer.getMaximumLatency();

        ImGui::Text("%s: %d", translations.translate("Packets sent").c_str(), stats.packets_sent);
        ImGui::Text(
            "%s: %d",
            translations.translate("Packets delivered").c_str(),
            stats.packets_delivered
        );
        ImGui::Text("%s: %d", translations.translate("Packets lost").c_str(), stats.packets_lost);

        ImGui::Separator();

        if (buffer.empty()) {
            ImGui::Text("%s: 0", translations.translate("Latency samples").c_str());
            ImGui::TextUnformatted(
                translations.translate(
                    "Waiting for delivered packets..."
                ).c_str()
            );
            return;
        }

        const std::vector<float> samples = buffer.values();

        ImGui::Text("%s: %zu", translations.translate("Latency samples").c_str(), samples.size());
        const std::string plot_label = translations.label("Latency (s)", "latency-plot");
        ImGui::PlotLines(
            plot_label.c_str(),
            samples.data(),
            static_cast<int>(samples.size()),
            0,
            nullptr,
            FLT_MAX,
            FLT_MAX,
            ImVec2(0, 120)
        );

        ImGui::Text("%s: %.6f", translations.translate("Minimum latency").c_str(), min_latency);
        ImGui::Text("%s: %.6f", translations.translate("Average latency").c_str(), avg_latency);
        ImGui::Text("%s: %.6f", translations.translate("Maximum latency").c_str(), max_latency);
    }
}
