#include "TrafficLabPanel.hpp"
#include "TranslationService.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "traffic/TrafficRun.hpp"
#include "traffic/TrafficPlanIO.hpp"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace gui {
    nlohmann::json TrafficFlowDraft::toJson(std::uint32_t id) const {
        nlohmann::json schedule;
        switch (pattern) {
            case TrafficPattern::Periodic:
                schedule = {{"kind", "periodic"}, {"interval_s", intervalMs / 1000}};
                break;
            case TrafficPattern::CBR:
                schedule = {{"kind", "cbr"}, {"rate_bps", rateMbps * 1e6}};
                break;
            case TrafficPattern::Burst:
                schedule = {{"kind", "burst"}, {"burst_packets", burstPackets},
                            {"burst_interval_s", burstIntervalMs / 1000},
                            {"packet_interval_s", packetIntervalMs / 1000}};
                break;
            case TrafficPattern::Poisson:
                schedule = {{"kind", "poisson"}, {"rate_pps", ratePps}};
                break;
            default:
                throw std::invalid_argument("Unknown Traffic Lab pattern");
        }
        nlohmann::json result{{"id", id}, {"source", source}, {"destination", destination},
            {"start_s", start}, {"end_s", start + duration}, {"wire_bytes", wireBytes},
            {"schedule", schedule}};
        if (deadline)
            result["deadline_s"] = deadlineMs / 1000;
        return result;
    }

    nlohmann::json TrafficLabPanel::plan() const {
        if (importedPlan_)
            return *importedPlan_;
        auto flows = nlohmann::json::array();
        for (std::size_t i = 0; i < flows_.size(); ++i)
            flows.push_back(flows_[i].toJson(static_cast<std::uint32_t>(i + 1)));
        return {{"schema_version", "1.0"}, {"seed", seed_}, {"max_packets", maximum_},
                {"flows", flows}};
    }

    std::optional<nlohmann::json> TrafficLabPanel::report(const kns::SimulationEngine& engine) const {
        if (engine_ != &engine || !run_)
            return std::nullopt;
        auto result = run_->toJson(engine.now());
        result["network_seed"] = networkSeed_;
        result["execution_interrupted"] = engine.executionState() == kns::ExecutionState::Cancelled ||
                                            engine.executionState() == kns::ExecutionState::Failed;
        return result;
    }

    bool TrafficLabPanel::render(kns::SimulationEngine& engine, TranslationService& tr,
                                 std::uint64_t networkSeed) {
        if (engine_ != &engine) {
            engine_ = &engine;
            run_.reset();
            error_.clear();
        }
        bool scheduled = false;
        const auto label = [&](const char* text, const char* id) { return tr.label(text, id); };
        if (!ImGui::Begin(label("Traffic Lab", "traffic-lab-window").c_str())) {
            ImGui::End();
            return false;
        }
        ImGui::TextWrapped("%s", tr.translate(
            "Configure concurrent abstract wire traffic. Schedule on a fresh simulation, then use Start or Resume in Stats.")
                .c_str());
        ImGui::BeginDisabled(run_ != nullptr);
        if (ImGui::Button(label("Paste plan JSON", "traffic-paste-plan").c_str())) {
            try {
                const char* text = ImGui::GetClipboardText();
                if (!text)
                    throw std::invalid_argument("Clipboard contains no plan text");
                std::size_t size = 0;
                while (size <= kns::traffic::MAX_PLAN_BYTES && text[size])
                    ++size;
                auto imported = kns::traffic::parseTrafficPlan(std::string_view(text, size), engine);
                importedPlan_ = std::move(imported);
                error_.clear();
            } catch (const std::exception& error) {
                error_ = error.what();
            }
        }
        if (importedPlan_) {
            if (ImGui::Button(label("Use editor drafts", "traffic-use-drafts").c_str())) {
                importedPlan_.reset();
                error_.clear();
            }
        }
        ImGui::EndDisabled();
        if (importedPlan_) {
            ImGui::TextWrapped("%s", tr.translate(
                "Imported plan (read-only). IDs, seeds, schedules and application settings are preserved.")
                    .c_str());
            ImGui::Text("%s: %zu", tr.translate("Flows").c_str(), importedPlan_->at("flows").size());
            ImGui::TextWrapped("%s: %llu", tr.translate("Traffic seed").c_str(),
                              importedPlan_->at("seed").get<unsigned long long>());
            ImGui::Text("%s: %llu", tr.translate("Packet budget").c_str(),
                        importedPlan_->at("max_packets").get<unsigned long long>());
            ImGui::TextWrapped("%s: %llu", tr.translate("Network seed").c_str(),
                        static_cast<unsigned long long>(networkSeed));
            if (ImGui::CollapsingHeader(label("Imported flows", "traffic-imported-flows").c_str()) &&
                ImGui::BeginTable("traffic-imported-preview", 4,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
                    ImGuiTableFlags_SizingFixedFit, {0, 140})) {
                for (const char* heading : {"Flow", "Source", "Destination", "Pattern"})
                    ImGui::TableSetupColumn(tr.translate(heading).c_str());
                ImGui::TableHeadersRow();
                for (const auto& flow : importedPlan_->at("flows")) {
                    ImGui::TableNextRow();
                    for (const char* key : {"id", "source", "destination"}) {
                        ImGui::TableNextColumn();
                        ImGui::Text("%llu", flow.at(key).get<unsigned long long>());
                    }
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(flow.at("schedule").at("kind").get_ref<const std::string&>().c_str());
                }
                ImGui::EndTable();
            }
        } else if (!run_ || ImGui::CollapsingHeader(
                label("Workload configuration", "traffic-configuration").c_str())) {
            ImGui::PushItemWidth(std::clamp(ImGui::GetContentRegionAvail().x * 0.48f, 80.0f, 220.0f));
            ImGui::BeginDisabled(run_ != nullptr);
            ImGui::InputScalar(label("Traffic seed", "traffic-seed").c_str(), ImGuiDataType_U64,
                                &seed_);
            ImGui::InputInt(label("Packet budget", "traffic-budget").c_str(), &maximum_);
            ImGui::Text("%s: %llu", tr.translate("Network seed").c_str(),
                        static_cast<unsigned long long>(networkSeed));
            ImGui::EndDisabled();
            if (ImGui::BeginCombo(label("Flow", "traffic-flow").c_str(),
                                  std::to_string(selected_ + 1).c_str())) {
                for (int i = 0; i < static_cast<int>(flows_.size()); ++i)
                    if (ImGui::Selectable(std::to_string(i + 1).c_str(), i == selected_))
                        selected_ = i;
                ImGui::EndCombo();
            }
            ImGui::BeginDisabled(run_ != nullptr);
            ImGui::BeginDisabled(flows_.size() >= 16);
            if (ImGui::Button(label("Add flow", "traffic-add").c_str())) {
                const auto copy = flows_[selected_];
                flows_.push_back(copy);
                selected_ = static_cast<int>(flows_.size()) - 1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(flows_.size() <= 1);
            if (ImGui::Button(label("Remove flow", "traffic-remove").c_str())) {
                flows_.erase(flows_.begin() + selected_);
                selected_ = std::min(selected_, static_cast<int>(flows_.size()) - 1);
            }
            ImGui::EndDisabled();
            auto& flow = flows_[selected_];
            ImGui::InputInt(label("Source node", "traffic-source").c_str(), &flow.source);
            ImGui::InputInt(label("Destination node", "traffic-destination").c_str(), &flow.destination);
            ImGui::InputInt(label("Wire bytes", "traffic-wire-bytes").c_str(), &flow.wireBytes);
            ImGui::InputDouble(label("Start (s)", "traffic-start").c_str(), &flow.start, 0, 0, "%.6f");
            ImGui::InputDouble(label("Duration (s)", "traffic-duration").c_str(), &flow.duration, 0, 0,
                                "%.6f");
            const std::array<std::string, 4> names{tr.translate("Periodic"), "CBR",
                                                  tr.translate("Burst"), "Poisson"};
            const std::array<const char*, 4> pointers{names[0].c_str(), names[1].c_str(),
                                                     names[2].c_str(), names[3].c_str()};
            int pattern = static_cast<int>(flow.pattern);
            if (ImGui::Combo(label("Pattern", "traffic-pattern").c_str(), &pattern, pointers.data(), 4))
                flow.pattern = static_cast<TrafficPattern>(pattern);
            if (flow.pattern == TrafficPattern::Periodic)
                ImGui::InputDouble(label("Interval (ms)", "traffic-interval").c_str(),
                                    &flow.intervalMs, 0, 0, "%.6f");
            else if (flow.pattern == TrafficPattern::CBR)
                ImGui::InputDouble(label("Offered rate (Mbps)", "traffic-mbps").c_str(),
                                    &flow.rateMbps, 0, 0, "%.6f");
            else if (flow.pattern == TrafficPattern::Poisson)
                ImGui::InputDouble(label("Rate (packets/s)", "traffic-pps").c_str(),
                                    &flow.ratePps, 0, 0, "%.6f");
            else {
                ImGui::InputInt(label("Packets per burst", "traffic-burst-packets").c_str(),
                                 &flow.burstPackets);
                ImGui::InputDouble(label("Burst interval (ms)", "traffic-burst-interval").c_str(),
                                    &flow.burstIntervalMs, 0, 0, "%.6f");
                ImGui::InputDouble(label("Packet spacing (ms)", "traffic-packet-interval").c_str(),
                                    &flow.packetIntervalMs, 0, 0, "%.6f");
            }
            ImGui::Checkbox(label("Packet deadline", "traffic-deadline-enabled").c_str(), &flow.deadline);
            if (flow.deadline)
                ImGui::InputDouble(label("Deadline (ms)", "traffic-deadline").c_str(),
                                    &flow.deadlineMs, 0, 0, "%.6f");
            ImGui::EndDisabled();

            ImGui::PopItemWidth();
        }

        const auto& stats = engine.getStats();
        const bool fresh = !run_ && engine.now() == 0 && !engine.hasEvents() &&
            engine.getTCPSessions().empty() && engine.executionStatistics().events_dispatched == 0 &&
            stats.packets_sent == 0 && stats.packets_lost == 0 && stats.packets_delivered == 0 &&
            engine.executionState() != kns::ExecutionState::Cancelled &&
            engine.executionState() != kns::ExecutionState::Failed;
        ImGui::BeginDisabled(!fresh);
        if (ImGui::Button(label("Schedule workload", "traffic-schedule").c_str())) {
            try {
                run_ = kns::traffic::TrafficRun::schedule(engine, plan());
                networkSeed_ = networkSeed;
                error_.clear();
                scheduled = true;
            } catch (const std::exception& error) {
                error_ = error.what();
            }
        }
        ImGui::EndDisabled();
        if (!fresh)
            ImGui::TextWrapped("%s", tr.translate(
                "Restart in Stats to schedule a new workload. Existing traffic and results are preserved until restart.")
                    .c_str());
        if (ImGui::Button(label("Copy plan JSON", "traffic-copy-plan").c_str())) {
            try {
                auto config = run_ ? run_->toJson(engine.now()).at("config") : plan();
                if (!run_)
                    kns::traffic::TrafficRun::validatePlan(engine, config);
                ImGui::SetClipboardText(config.dump(2).c_str());
                error_.clear();
            } catch (const std::exception& error) {
                error_ = error.what();
            }
        }
        if (!error_.empty())
            ImGui::TextWrapped("%s", error_.c_str());
        if (const auto current = report(engine)) {
            ImGui::Separator();
            const bool limited = current->at("generation_limit_reached").get<bool>();
            const bool pending = current->at("generation_pending").get<bool>() ||
                                  current->at("pending").get<std::uint64_t>() != 0;
            const char* status = current->at("execution_interrupted").get<bool>() ? "Interrupted"
                                 : limited ? "Packet budget reached (partial workload)"
                                 : pending ? "Work pending" : "Workload drained";
            ImGui::TextWrapped("%s", tr.translate(status).c_str());
            for (const auto& [title, key] : {std::pair{"Generated", "generated"},
                    std::pair{"Delivered", "delivered"}, std::pair{"Dropped", "dropped"},
                    std::pair{"Pending", "pending"}})
                ImGui::Text("%s: %llu", tr.translate(title).c_str(),
                            current->at(key).get<unsigned long long>());
            const float tableHeight = std::min(200.0f,
                (static_cast<float>(current->at("flows").size()) + 1) *
                ImGui::GetTextLineHeightWithSpacing() + 12);
            if (ImGui::BeginTable("traffic-outcomes", 7,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_SizingFixedFit, {0, tableHeight})) {
                for (const char* heading : {"Flow", "Generated", "Delivered", "Dropped", "Pending",
                                             "On time", "Late"})
                    ImGui::TableSetupColumn(tr.translate(heading).c_str());
                ImGui::TableHeadersRow();
                for (const auto& result : current->at("flows")) {
                    ImGui::TableNextRow();
                    for (const char* key : {"id", "generated", "delivered", "dropped", "pending",
                                             "delivered_on_time", "delivered_late"}) {
                        ImGui::TableNextColumn();
                        const bool deadlineColumn = std::string_view(key).starts_with("delivered_");
                        if (deadlineColumn && !result.at("config").contains("deadline_s"))
                            ImGui::TextUnformatted("--");
                        else
                            ImGui::Text("%llu", result.at(key).get<unsigned long long>());
                    }
                }
                ImGui::EndTable();
            }
            bool hasOperations = false;
            for (const auto& flow : current->at("flows"))
                hasOperations |= flow.at("config").contains("application") &&
                    flow.at("config").at("application").contains("response_wire_bytes");
            if (hasOperations) {
                ImGui::TextUnformatted(tr.translate("Request/response operations").c_str());
                if (ImGui::BeginTable("traffic-operations", 8,
                    ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingFixedFit, {0, tableHeight})) {
                    for (const char* heading : {"Flow", "Started", "Completed", "Failed", "Pending",
                                                 "On time", "Late", "P95 (ms)"})
                        ImGui::TableSetupColumn(tr.translate(heading).c_str());
                    ImGui::TableHeadersRow();
                    for (const auto& flow : current->at("flows")) {
                        const auto& config = flow.at("config");
                        if (!config.contains("application") ||
                            !config.at("application").contains("response_wire_bytes"))
                            continue;
                        ImGui::TableNextRow();
                        for (const char* key : {"id", "operations_started", "operations_completed",
                            "operations_failed", "operations_pending", "operations_on_time", "operations_late"}) {
                            ImGui::TableNextColumn();
                            const bool deadline = std::string_view(key) == "operations_on_time" ||
                                                  std::string_view(key) == "operations_late";
                            if (deadline && !config.at("application").contains("deadline_s"))
                                ImGui::TextUnformatted("--");
                            else
                                ImGui::Text("%llu", flow.at(key).get<unsigned long long>());
                        }
                        ImGui::TableNextColumn();
                        const auto& latency = flow.at("operation_latency");
                        if (latency.at("p95_s").is_null())
                            ImGui::TextUnformatted("--");
                        else
                            ImGui::Text("%.3f", latency.at("p95_s").get<double>() * 1000);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s: %s", tr.translate("Percentile scope").c_str(),
                                latency.at("percentile_scope").get_ref<const std::string&>().c_str());
                    }
                    ImGui::EndTable();
                }
            }
            if (ImGui::Button(label("Copy report JSON", "traffic-copy-report").c_str()))
                ImGui::SetClipboardText(current->dump(2).c_str());
        }
        ImGui::TextWrapped("%s", tr.translate(
            "Abstract wire messages, not TCP or real sockets. Offered rate is not guaranteed throughput. A drained workload can still have losses or missed deadlines.")
                .c_str());
        ImGui::End();
        return scheduled;
    }
}
