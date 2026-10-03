#include "gui/include/TopologyPannel.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <stdexcept>
#include <cstdio>
#include <optional>

#include "imgui.h"

#include "engine/core/SimulationEngine.hpp"
#include "gui/include/TranslationService.hpp"
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

    void TopologyPanel::render(
        kns::SimulationEngine& engine,
        TranslationService& translations
    )
    {
        const std::string window_label =
            translations.label("Topology", "topology-window");
        ImGui::Begin(window_label.c_str());

        auto& topology = engine.getTopology();
        const auto& links = topology.getLinks();

        std::array<const char*, kns::deviceTypeNames.size()> type_names{};
        for (std::size_t index = 0; index < type_names.size(); ++index) {
            type_names[index] = kns::deviceTypeNames[index].data();
        }
        ImGui::InputText(translations.label("Device label", "device-label").c_str(), new_label_.data(), new_label_.size());
        ImGui::Combo(translations.label("Device type", "device-type").c_str(), &new_type_, type_names.data(), static_cast<int>(type_names.size()));
        if (ImGui::Button(translations.label("Add device", "add-device").c_str())) {
            if (topology.size() < 4096) {
                const int id = engine.createNode();
                topology.setNodeLabel(id, new_label_.data());
                kns::DeviceInfo info;
                info.type = static_cast<kns::DeviceType>(new_type_);
                info.evidence = "user_defined";
                topology.setNodeDeviceInfo(id, std::move(info));
                new_label_.fill(0);
                error_.clear();
            } else {
                error_ = "Node limit reached; reload to start a new simulation.";
            }
        }
        int remove_node = -1;
        if (ImGui::BeginTable("TopologyDevices", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn(translations.translate("Label").c_str());
            ImGui::TableSetupColumn(translations.translate("Type").c_str());
            ImGui::TableSetupColumn(translations.translate("Action").c_str());
            ImGui::TableHeadersRow();
            for (int id = 0; id < topology.size(); ++id) {
                const auto& node = *topology.getNode(id);
                if (!node.isActive()) continue;
                ImGui::PushID(id);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", id);
                ImGui::TableSetColumnIndex(1);
                std::array<char, 256> label{};
                std::snprintf(label.data(), label.size(), "%s", node.getLabel().c_str());
                if (ImGui::InputText("##label", label.data(), label.size())) topology.setNodeLabel(id, label.data());
                ImGui::TableSetColumnIndex(2);
                int type = static_cast<int>(node.getDeviceInfo().type);
                if (ImGui::Combo("##type", &type, type_names.data(), static_cast<int>(type_names.size()))) {
                    auto device = node.getDeviceInfo();
                    device.type = static_cast<kns::DeviceType>(type);
                    device.evidence = "user_override";
                    topology.setNodeDeviceInfo(id, std::move(device));
                }
                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton(translations.label("Remove", "remove-device").c_str())) remove_node = id;
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (remove_node >= 0) engine.deleteNode(remove_node);

        ImGui::InputInt(translations.label("Link from", "link-from").c_str(), &link_from_);
        ImGui::InputInt(translations.label("Link to", "link-to").c_str(), &link_to_);
        if (ImGui::Button(translations.label("Add link", "add-link").c_str())) {
            try {
                const auto* from = topology.getNode(link_from_);
                const auto* to = topology.getNode(link_to_);
                if (!from || !to || !from->isActive() || !to->isActive()) throw std::invalid_argument("Select two existing active device IDs.");
                engine.createLink(link_from_, link_to_, 100.0, 1.0);
                error_.clear();
            } catch (const std::exception& exception) {
                error_ = exception.what();
            }
        }
        if (!error_.empty()) ImGui::TextWrapped("%s", error_.c_str());
        ImGui::TextWrapped("Device types describe the topology; TCP behavior is shared. Segment connections are inferred, with assumed simulation metrics.");
        ImGui::Separator();

        const std::array<std::string, 4> metric_labels{
            translations.translate("Delay"),
            translations.translate("Bandwidth"),
            translations.translate("Hop count"),
            translations.translate("Delay / bandwidth")
        };
        const std::array<const char*, 4> metric_label_pointers{
            metric_labels[0].c_str(),
            metric_labels[1].c_str(),
            metric_labels[2].c_str(),
            metric_labels[3].c_str()
        };
        constexpr std::array<kns::RoutingMetric, 4> metrics{
            kns::RoutingMetric::Delay,
            kns::RoutingMetric::Bandwidth,
            kns::RoutingMetric::HopCount,
            kns::RoutingMetric::DelayBandwidth
        };

        int selected_metric = routingMetricIndex(engine.getRoutingMetric());
        const std::string routing_metric_label =
            translations.label("Routing metric", "routing-metric");
        if (ImGui::Combo(
                routing_metric_label.c_str(),
                &selected_metric,
                metric_label_pointers.data(),
                static_cast<int>(metric_label_pointers.size())
            )) {
            engine.setRoutingMetric(
                metrics[static_cast<std::size_t>(selected_metric)]
            );
        }

        if (links.empty()) {
            ImGui::TextDisabled(
                "%s",
                translations.translate("No links in the topology.").c_str()
            );
            ImGui::End();
            return;
        }

        bool routing_changed = false;
        std::optional<std::uint64_t> remove_link;

        if (ImGui::BeginTable(
                "TopologyLinksTable",
                7,
                ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(translations.translate("Link").c_str());
            ImGui::TableSetupColumn(translations.translate("Bandwidth (Mbps)").c_str());
            ImGui::TableSetupColumn(translations.translate("Delay (ms)").c_str());
            ImGui::TableSetupColumn(translations.translate("Loss").c_str());
            ImGui::TableSetupColumn(
                translations.translate("Up").c_str(),
                ImGuiTableColumnFlags_WidthFixed
            );
            ImGui::TableSetupColumn(translations.translate("Queue capacity").c_str());
            ImGui::TableSetupColumn(translations.translate("Action").c_str());
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
                if (link->isInferred() && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Inferred adjacency: %s. Metrics are simulation assumptions.", link->getEvidence().c_str());
                }
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
                ImGui::TableSetColumnIndex(6);
                if (ImGui::SmallButton(translations.label("Remove", "remove-link").c_str())) remove_link = link->getId();
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

        if (remove_link) engine.deleteLinkById(*remove_link);
        if (routing_changed) {
            engine.rebuildRoutingTables();
        }

        ImGui::End();
    }

} // namespace gui
