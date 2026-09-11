#include "gui/include/TopologyPannel.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <stdexcept>

#include "imgui.h"

#include "engine/core/SimulationEngine.hpp"
#include "network/Link.hpp"
#include "network/Topology.hpp"

namespace gui {
    namespace {
        double positiveValue(double value, double fallback) noexcept
        {
            return std::isfinite(value) && value > 0.0 ? value : fallback;
        }

        double nonNegativeValue(double value, double fallback) noexcept
        {
            return std::isfinite(value) && value >= 0.0 ? value : fallback;
        }

        int routingMetricIndex(kns::RoutingMetric metric) noexcept
        {
            switch (metric) {
                case kns::RoutingMetric::Delay:
                    return 0;
                case kns::RoutingMetric::Bandwidth:
                    return 1;
                case kns::RoutingMetric::HopCount:
                    return 2;
                case kns::RoutingMetric::DelayBandwidth:
                    return 3;
            }

            return 0;
        }
    } // namespace

    void TopologyPanel::render(kns::SimulationEngine& engine)
    {
        ImGui::Begin("Topology");

        auto& topology = engine.getTopology();
        const auto& links = topology.getLinks();

        constexpr std::array<const char*, 4> metric_labels{
            "Delay",
            "Bandwidth",
            "Hop count",
            "Delay / bandwidth"
        };
        constexpr std::array<kns::RoutingMetric, 4> metrics{
            kns::RoutingMetric::Delay,
            kns::RoutingMetric::Bandwidth,
            kns::RoutingMetric::HopCount,
            kns::RoutingMetric::DelayBandwidth
        };

        int selected_metric = routingMetricIndex(engine.getRoutingMetric());
        if (ImGui::Combo(
                "Routing metric",
                &selected_metric,
                metric_labels.data(),
                static_cast<int>(metric_labels.size())
            )) {
            engine.setRoutingMetric(
                metrics[static_cast<std::size_t>(selected_metric)]
            );
        }

        if (links.empty()) {
            ImGui::TextDisabled("No links in the topology.");
            ImGui::End();
            return;
        }

        bool routing_changed = false;

        if (ImGui::BeginTable(
                "TopologyLinksTable",
                6,
                ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Link");
            ImGui::TableSetupColumn("Bandwidth (Mbps)");
            ImGui::TableSetupColumn("Delay (ms)");
            ImGui::TableSetupColumn("Loss");
            ImGui::TableSetupColumn("Up", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Queue capacity");
            ImGui::TableHeadersRow();

            for (const auto& link : links) {
                if (!link) {
                    continue;
                }

                const std::string id = std::to_string(link->getId());
                double bandwidth = link->getBandwidthMbps();
                double delay = link->getDelayMs();
                double loss_percent = link->getLossProb() * 100.0;
                bool up = link->isUp();

                ImGui::PushID(id.c_str());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d <-> %d", link->getA(), link->getB());
                ImGui::TableSetColumnIndex(1);
                const bool bandwidth_changed = ImGui::InputDouble(
                    "##bandwidth",
                    &bandwidth,
                    0.0,
                    0.0,
                    "%.3f"
                );
                ImGui::TableSetColumnIndex(2);
                const bool delay_changed = ImGui::InputDouble(
                    "##delay",
                    &delay,
                    0.0,
                    0.0,
                    "%.3f"
                );
                ImGui::TableSetColumnIndex(3);
                const bool loss_changed = ImGui::InputDouble(
                    "##loss",
                    &loss_percent,
                    0.0,
                    0.0,
                    "%.2f%%"
                );
                ImGui::TableSetColumnIndex(4);
                const bool up_changed = ImGui::Checkbox("##up", &up);
                ImGui::TableSetColumnIndex(5);
                int capacity = static_cast<int>(link->getQueueCapacity());
                if (ImGui::InputInt("##capacity", &capacity)) {
                    try {
                        link->setQueueCapacity(capacity);
                    } catch (const std::invalid_argument& error) {
                        ImGui::SetTooltip("%s", error.what());
                    }
                }
                ImGui::PopID();

                if (bandwidth_changed) {
                    link->setBandwidthMbps(
                        positiveValue(bandwidth, link->getBandwidthMbps())
                    );
                    routing_changed = true;
                }

                if (delay_changed) {
                    link->setDelayMs(
                        nonNegativeValue(delay, link->getDelayMs())
                    );
                    routing_changed = true;
                }

                if (loss_changed) {
                    const double loss_probability = std::clamp(
                        loss_percent / 100.0,
                        0.0,
                        1.0
                    );
                    link->setLossProb(loss_probability);
                }

                if (up_changed) {
                    link->setUp(up);
                    routing_changed = true;
                }
            }

            ImGui::EndTable();
        }

        if (routing_changed) {
            engine.rebuildRoutingTables();
        }

        ImGui::End();
    }

} // namespace gui
