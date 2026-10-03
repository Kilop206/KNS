#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "gui/include/NativeFileDialog.hpp"
#include "include/Environment.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <deque>
#include <sstream>
#include <iomanip>

#include "analysis/AIContextBuilder.hpp"
#include "analysis/AnalysisJsonSerializer.hpp"
#include "analysis/NetworkAnalysis.hpp"
#include "analysis/NetworkAnalyzer.hpp"
#include "intelligence/IntelligenceClient.hpp"
#include "intelligence/IntelligenceConfigLoader.hpp"
#include "intelligence/IntelligenceRequestBuilder.hpp"
#include "hub/TopologyHubClient.hpp"
#include "engine/core/Random.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "engine/core/SimulationState.hpp"
#include "engine/core/Stats.hpp"
#include "engine/core/RunConfig.hpp"
#include "gui/include/GUIFormat.hpp"
#include "gui/include/IntelligencePanel.hpp"
#include "gui/include/LatencyChart.hpp"
#include "gui/include/MetricsPannel.hpp"
#include "gui/include/PacketRenderer.hpp"
#include "gui/include/TcpCongestionPanel.hpp"
#include "gui/include/TcpConnectionPanel.hpp"
#include "gui/include/TopologyPannel.hpp"
#include "gui/include/TopologyCanvas.hpp"
#include "gui/include/VisualPacketManager.hpp"
#include "gui/include/VisualPacket.hpp"
#include "gui/include/Window.hpp"
#include "gui/include/ThemeManager.hpp"
#include "gui/include/TranslationService.hpp"
#include "network/Packet.hpp"
#include "network/Routing.hpp"
#include "network/Topology.hpp"
#include "network/TopologyLoader.hpp"
#include "network/LiveTopologyWatcher.hpp"

using namespace kns;
using namespace gui;

namespace fs = std::filesystem;

constexpr double kBasePacketsPerSecond = 1.0;
constexpr double kBasePacketsPerMinute = kBasePacketsPerSecond * 60.0;
constexpr double kSimToVisualScale     = 20.0;

namespace {

    void printUsage(std::ostream& output)
    {
        output
            << "Usage:\n"
            << "  KNS [topology.json]\n"
            << "  KNS --hub-topology <id>\n"
            << "  KNS --headless (--topology <file> | --hub-topology <id>) [--output <csv>] "
            << "[--routing-metric <metric>]\n\n"
            << "Options:\n"
            << "  --headless                 Run without the graphical interface\n"
            << "  --topology <file>          Load a topology JSON file\n"
            << "  --watch-topology <file>    Load and continuously synchronize a topology (GUI)\n"
            << "  --hub-topology <id>        Load a public topology from Topology Hub\n"
            << "  --output <csv>             Write headless statistics to a CSV file\n"
            << "  --routing-metric <metric>  Select the headless routing metric\n"
            << "  --seed <integer>           Random seed (default: 42)\n"
            << "  --packet-size <bytes>      Positive packet size (default: 1500)\n"
            << "  -h, --help                 Show this help message\n\n"
            << "Headless routing metrics:\n"
            << "  delay (default), bandwidth, hop-count, delay-bandwidth\n";
    }

    [[nodiscard]] bool isAutoStartEnabledValue(
        std::string_view value
    ) noexcept
    {
        return value != "0" &&
            value != "false" &&
            value != "False";
    }

    [[nodiscard]] bool autoStartFromEnvironment(
        bool default_value
    ) noexcept
    {
        const auto raw_value =
            kns::app::readEnvironmentVariable("KNS_AUTO_START");

        return !raw_value.has_value()
            ? default_value
            : isAutoStartEnabledValue(*raw_value);
    }

    [[nodiscard]] kns::app::hub::TopologyHubClientConfig topologyHubConfigFromEnvironment()
    {
        kns::app::hub::TopologyHubClientConfig config;
        if (const auto base_url =
                kns::app::readEnvironmentVariable("KNS_TOPOLOGY_HUB_URL")) {
            config.base_url = *base_url;
        }
        if (const auto token =
                kns::app::readEnvironmentVariable("KNS_TOPOLOGY_HUB_TOKEN")) {
            config.bearer_token = *token;
        }
        return config;
    }

    [[nodiscard]] kns::app::hub::HubTopology loadTopologyFromHub(
        const std::string& topology_id
    )
    {
        return kns::app::hub::TopologyHubClient(topologyHubConfigFromEnvironment())
            .fetchTopology(topology_id);
    }

} // namespace

struct LogEntry {
    double time = 0.0;
    kns::PacketType type = kns::PacketType::DATA;
    int from = -1;
    int to = -1;
    std::uint64_t session_id = 0;
    std::string text;
};

struct EventLog {
    std::deque<LogEntry> lines;
    std::size_t max_lines = 300;

    void add(
        double time,
        kns::PacketType type,
        int from,
        int to,
        std::uint64_t session_id,
        std::string line
    ) {
        lines.push_back(LogEntry{
            time,
            type,
            from,
            to,
            session_id,
            std::move(line)
        });

        if (lines.size() > max_lines) {
            lines.pop_front();
        }
    }

    void clear() noexcept
    {
        lines.clear();
    }
};

static const char* tcpStateToString(kns::TCPState state)
{
    switch (state)
    {
        case kns::TCPState::CLOSED:       return "CLOSED";
        case kns::TCPState::SYN_SENT:     return "SYN_SENT";
        case kns::TCPState::SYN_RECEIVED: return "SYN_RECEIVED";
        case kns::TCPState::ESTABLISHED:  return "ESTABLISHED";
        case kns::TCPState::FIN_WAIT_1:   return "FIN_WAIT_1";
        case kns::TCPState::FIN_WAIT_2:   return "FIN_WAIT_2";
        case kns::TCPState::CLOSE_WAIT:   return "CLOSE_WAIT";
        case kns::TCPState::LAST_ACK:     return "LAST_ACK";
        case kns::TCPState::TIME_WAIT:    return "TIME_WAIT";
        case kns::TCPState::CLOSING:      return "CLOSING";
        default:                          return "UNKNOWN";
    }
}

static ImU32 tcpStateColor(kns::TCPState state)
{
    switch (state)
    {
        case kns::TCPState::ESTABLISHED:
            return IM_COL32(200, 255, 200, 255);

        case kns::TCPState::SYN_SENT:
            return IM_COL32(255, 240, 180, 255);

        case kns::TCPState::SYN_RECEIVED:
            return IM_COL32(255, 225, 190, 255);

        case kns::TCPState::CLOSE_WAIT:
            return IM_COL32(255, 230, 180, 255);

        case kns::TCPState::LAST_ACK:
            return IM_COL32(225, 205, 255, 255);

        case kns::TCPState::TIME_WAIT:
            return IM_COL32(190, 225, 255, 255);

        case kns::TCPState::CLOSED:
            return IM_COL32(220, 220, 220, 255);

        default:
            return IM_COL32(255, 255, 255, 255);
    }
}

static void renderTCPSessionsWindow(
    const kns::SimulationEngine& engine,
    TranslationService& translations
)
{
    const std::string window_label =
        translations.label("TCP Sessions", "tcp-sessions-window");
    ImGui::Begin(window_label.c_str());

    const auto& sessions = engine.getTCPSessions();

    if (ImGui::BeginTable(
        "TCPSessionsTable",
        4,
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(translations.translate("Session").c_str());
        ImGui::TableSetupColumn(translations.translate("Source").c_str());
        ImGui::TableSetupColumn(translations.translate("Destination").c_str());
        ImGui::TableSetupColumn(translations.translate("State").c_str());
        ImGui::TableHeadersRow();

        for (const auto& [id, session] : sessions)
        {
            ImGui::TableNextRow();

            ImGui::TableSetBgColor(
                ImGuiTableBgTarget_RowBg0,
                tcpStateColor(session.getState())
            );

            ImGui::TableSetColumnIndex(0);
            ImGui::Text(
                "%llu",
                static_cast<unsigned long long>(
                    session.getSession_id()
                )
            );

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%i", session.getSource());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%i", session.getDestination());

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(
                translations.translate(
                    tcpStateToString(session.getState())
                ).c_str()
            );
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

static void renderEventLogWindow(
    const EventLog& log,
    TranslationService& translations
)
{
    const std::string window_label =
        translations.label("Event Log", "event-log-window");
    ImGui::Begin(window_label.c_str());

    if (ImGui::BeginTable(
        "EventLogTable",
        6,
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY,
        ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn(translations.translate("Time").c_str());
        ImGui::TableSetupColumn(translations.translate("Type").c_str());
        ImGui::TableSetupColumn(translations.translate("From").c_str());
        ImGui::TableSetupColumn(translations.translate("To").c_str());
        ImGui::TableSetupColumn(translations.translate("Session").c_str());
        ImGui::TableSetupColumn(translations.translate("Details").c_str());
        ImGui::TableHeadersRow();

        for (const auto& entry : log.lines)
        {
            ImGui::TableNextRow();

            ImU32 rowColor = IM_COL32(255, 255, 255, 255);

            switch (entry.type)
            {
                case kns::PacketType::SYN:
                    rowColor = IM_COL32(255, 240, 170, 255);
                    break;

                case kns::PacketType::SYN_ACK:
                    rowColor = IM_COL32(225, 205, 255, 255);
                    break;

                case kns::PacketType::ACK:
                    rowColor = IM_COL32(190, 225, 255, 255);
                    break;

                case kns::PacketType::DATA:
                    rowColor = IM_COL32(190, 245, 200, 255);
                    break;

                case kns::PacketType::FIN:
                    rowColor = IM_COL32(255, 205, 190, 255);
                    break;

                case kns::PacketType::RST:
                    rowColor = IM_COL32(255, 180, 180, 255);
                    break;

                default:
                    break;
            }

            ImGui::TableSetBgColor(
                ImGuiTableBgTarget_RowBg0,
                rowColor
            );

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%.3f", entry.time);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(
                PacketRenderer::packetTypeToString(entry.type)
            );

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", entry.from);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", entry.to);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text(
                "%llu",
                static_cast<unsigned long long>(
                    entry.session_id
                )
            );

            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(entry.text.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

static std::vector<std::pair<int, int>> buildConnectionPlan(
    const Topology& topo
)
{
    std::set<std::pair<int, int>> connections;

    for (std::size_t i = 0;
         i < static_cast<std::size_t>(topo.size());
         ++i)
    {
        for (const auto& link :
             topo.getLinksFromNode(static_cast<int>(i)))
        {
            if (link &&
                link->getA() >= 0 &&
                link->getB() >= 0)
            {
                connections.insert({
                    link->getA(),
                    link->getB()
                });
            }
        }
    }

    return {
        connections.begin(),
        connections.end()
    };
}

static void generatePackets(
    std::unique_ptr<SimulationEngine>& engine,
    const Topology& topo
)
{
    if (!engine || topo.size() <= 0) {
        return;
    }

    const auto plan = buildConnectionPlan(topo);

    for (const auto& [from, to] : plan) {
        engine->startTCPConnection(from, to);
    }
}

static void renderStatsWindow(
    SimulationEngine& engine,
    TranslationService& translations,
    SimulationState& state,
    const Stats& stats,
    CircularBuffer& buffer,
    int& packetSize,
    float& lossProb,
    bool& lossOverride,
    float& speedMultiplier,
    bool& stepRequested,
    bool engineHasEvents,
    bool& restartRequested
)
{
    const auto label = [&translations](
        std::string_view english,
        std::string_view stable_id
    ) {
        return translations.label(english, stable_id);
    };

    ImGui::Begin(label("Stats", "stats-window").c_str());

    const std::span<const UiLanguageOption> language_options =
        TranslationService::languageOptions();
    std::size_t selected_language = 0;
    for (std::size_t i = 0; i < language_options.size(); ++i) {
        if (language_options[i].language == translations.getLanguage()) {
            selected_language = i;
            break;
        }
    }

    if (ImGui::BeginCombo(
            label("Language", "interface-language").c_str(),
            language_options[selected_language].display_name.data()
        )) {
        for (std::size_t i = 0; i < language_options.size(); ++i) {
            const bool selected = i == selected_language;
            if (ImGui::Selectable(
                    language_options[i].display_name.data(),
                    selected
                )) {
                translations.setLanguage(language_options[i].language);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (translations.isTranslating()) {
        ImGui::TextDisabled(
            "%s",
            translations.translate("Translating interface...").c_str()
        );
    }

    if (const std::string error = translations.getLastError(); !error.empty()) {
        ImGui::TextWrapped(
            "%s: %s",
            translations.translate("Translation unavailable").c_str(),
            error.c_str()
        );
        if (ImGui::SmallButton(label("Retry", "retry-translation").c_str())) {
            translations.retry();
        }
    }

    if (ImGui::CollapsingHeader(
        label("Simulation", "simulation-section").c_str(),
        ImGuiTreeNodeFlags_DefaultOpen))
    {
        // --------------------------------------------------
        // READY
        // --------------------------------------------------

        if (state == SimulationState::Ready)
        {
            ImGui::TextDisabled(
                "%s",
                translations.translate("Simulation ready.").c_str()
            );

            ImGui::BeginDisabled(!engineHasEvents);

            if (ImGui::Button(label("Start", "start-simulation").c_str())) {
                state = SimulationState::Running;
            }

            ImGui::EndDisabled();
        }

        // --------------------------------------------------
        // RUNNING
        // --------------------------------------------------

        else if (state == SimulationState::Running)
        {
            if (ImGui::Button(label("Pause", "pause-simulation").c_str()))
            {
                state =
                    SimulationState::Paused;
            }
        }

        // --------------------------------------------------
        // PAUSED
        // --------------------------------------------------

        else if (state == SimulationState::Paused)
        {
            if (ImGui::Button(label("Resume", "resume-simulation").c_str()))
            {
                if (engineHasEvents)
                {
                    state =
                        SimulationState::Running;
                }
            }

            if (engineHasEvents)
            {
                ImGui::SameLine();

                if (ImGui::Button(label("Step", "step-simulation").c_str()))
                {
                    stepRequested = true;
                }
            }
        }

        // --------------------------------------------------
        // FINISHED
        // --------------------------------------------------

        else if (state == SimulationState::Finished)
        {
            ImGui::TextDisabled(
                "%s",
                translations.translate("Simulation finished.").c_str()
            );
        }

        if (ImGui::Button(label("Restart", "restart-simulation").c_str())) {
            restartRequested = true;
        }

        ImGui::SliderFloat(
            label("Simulation speed", "simulation-speed").c_str(),
            &speedMultiplier,
            0.18f,
            4.0f,
            "%.2fx"
        );

        if (state == SimulationState::Ready &&
            !engineHasEvents)
        {
            ImGui::TextDisabled(
                "%s",
                translations.translate("No traffic scheduled.").c_str()
            );
        }
    }

    // ------------------------------------------------------
    // Traffic
    // ------------------------------------------------------

    if (ImGui::CollapsingHeader(
        label("Traffic", "traffic-section").c_str(),
        ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox(label("Override link loss", "loss-override").c_str(), &lossOverride)) {
            if (lossOverride) engine.setGlobalLossProb(lossProb);
            else engine.clearGlobalLossOverride();
        }
        ImGui::BeginDisabled(!lossOverride);
        float lossPercent =
            lossProb * 100.0f;

        if (ImGui::SliderFloat(
            label("Loss probability", "loss-probability").c_str(),
            &lossPercent,
            0.0f,
            100.0f,
            "%.0f %%"))
        {
            lossProb =
                lossPercent / 100.0f;

            engine.setGlobalLossProb(
                lossProb
            );
        }

        ImGui::EndDisabled();

        if (ImGui::SliderInt(
            label("Packet size (bytes)", "packet-size").c_str(),
            &packetSize,
            64,
            65535,
            "%d B"))
        {
            engine.setGlobalPacketSize(
                packetSize
            );
        }

        ImGui::Text(
            "%s: %s",
            translations.translate("Current size").c_str(),
            formatBytes(packetSize).c_str()
        );
    }

    // ------------------------------------------------------
    // Statistics
    // ------------------------------------------------------

    if (ImGui::CollapsingHeader(
        label("Statistics", "statistics-section").c_str(),
        ImGuiTreeNodeFlags_DefaultOpen))
    {
        MetricsPannel panel;
        panel.render(stats, buffer, translations);
    }

    // ------------------------------------------------------
    // Congestion Control
    // ------------------------------------------------------

    if (ImGui::CollapsingHeader(
        label("TCP Congestion Control", "tcp-congestion-section").c_str(),
        ImGuiTreeNodeFlags_DefaultOpen))
    {
        TcpCongestionPanel panel;
        panel.render(engine, translations);
    }

    // ------------------------------------------------------
    // Network configuration
    // ------------------------------------------------------

    if (ImGui::CollapsingHeader(
        label("Network Configuration", "network-configuration-section").c_str()))
    {
        ImGui::Text(
            "%s: %.0f %s",
            translations.translate("Base rate").c_str(),
            kBasePacketsPerMinute,
            translations.translate("packets/min at 1.0x").c_str()
        );

        ImGui::Text(
            "%s: %d",
            translations.translate("Packets per route").c_str(),
            engine.getPacketsPerRoute()
        );
    }

    // ------------------------------------------------------
    // Validation
    // ------------------------------------------------------

    if (ImGui::CollapsingHeader(
        label("Validation", "validation-section").c_str()))
    {
        ImGui::Text(
            "%s: %d",
            translations.translate("Packets sent").c_str(),
            stats.packets_sent
        );

        ImGui::Text(
            "%s: %d",
            translations.translate("Packets delivered").c_str(),
            stats.packets_delivered
        );

        ImGui::Text(
            "%s: %d",
            translations.translate("Packets lost").c_str(),
            stats.packets_lost
        );
    }

    ImGui::End();
}

static void renderSelectedNodePanel(
    const Topology& topo,
    int selected_node,
    std::span<const Routing::RoutingEntry> routingTable,
    TranslationService& translations
)
{
    const std::string window_label =
        translations.label("Node Details", "node-details-window");
    ImGui::Begin(window_label.c_str());

    if (selected_node < 0)
    {
        ImGui::TextUnformatted(
            translations.translate("No node selected.").c_str()
        );
        ImGui::End();
        return;
    }

    ImGui::Text(
        "%s: %d",
        translations.translate("Selected node").c_str(),
        selected_node
    );

    ImGui::Separator();

    if (selected_node >= topo.size() || !topo.getNode(selected_node)->isActive())
    {
        ImGui::TextUnformatted(translations.translate("Invalid node.").c_str());
        ImGui::End();
        return;
    }

    const auto& selected = *topo.getNode(selected_node);
    const auto& device = selected.getDeviceInfo();
    ImGui::TextWrapped("%s", selected.getLabel().c_str());
    ImGui::Text("Device type: %s", toString(device.type).data());
    if (!device.mac.empty()) ImGui::Text("MAC: %s", device.mac.c_str());
    for (const auto& address : device.addresses) ImGui::Text("IP: %s", address.c_str());
    if (!device.evidence.empty()) ImGui::TextWrapped("Evidence: %s", device.evidence.c_str());
    if (!device.external_id.empty()) ImGui::TextWrapped("Identity: %s", device.external_id.c_str());
    ImGui::Separator();
    ImGui::Text("%s:", translations.translate("Neighbors").c_str());

    const auto& links =
        topo.getLinksFromNode(selected_node);

    bool hasNeighbors = false;

    for (const auto& link : links)
    {
        if (!link) {
            continue;
        }

        const int other =
            link->getOtherNode(selected_node);

        if (other != -1)
        {
            hasNeighbors = true;
            ImGui::BulletText(
                "%d",
                other
            );
        }
    }

    if (!hasNeighbors)
    {
        ImGui::TextDisabled(
            "%s",
            translations.translate("No neighbors.").c_str()
        );
    }

    ImGui::Separator();
    ImGui::Text("%s:", translations.translate("Routing table").c_str());

    if (routingTable.empty())
    {
        ImGui::TextDisabled(
            "%s",
            translations.translate("Routing table is empty.").c_str()
        );
    }
    else
    {
        for (const auto& entry : routingTable)
        {
            if (entry.distance ==
                std::numeric_limits<double>::infinity())
            {
                ImGui::Text(
                    "%s: %d | %s: - | %s: inf",
                    translations.translate("Destination").c_str(),
                    entry.destination,
                    translations.translate("Next hop").c_str(),
                    translations.translate("Distance").c_str()
                );
            }
            else
            {
                ImGui::Text(
                    "%s: %d | %s: %d | %s: %.2f",
                    translations.translate("Destination").c_str(),
                    entry.destination,
                    translations.translate("Next hop").c_str(),
                    entry.next_hop,
                    translations.translate("Distance").c_str(),
                    entry.distance
                );
            }
        }
    }

    ImGui::End();
}

static void SetupDockingLayout()
{
    const ImGuiID dockspace_id =
        ImGui::GetID("MainDockSpace");

    ImGui::DockBuilderRemoveNode(
        dockspace_id
    );

    ImGui::DockBuilderAddNode(
        dockspace_id,
        ImGuiDockNodeFlags_DockSpace
    );

    ImGui::DockBuilderSetNodeSize(
        dockspace_id,
        ImGui::GetMainViewport()->WorkSize
    );

    ImGuiID dock_main =
        dockspace_id;

    ImGuiID dock_left =
        0;

    ImGuiID dock_right =
        0;

    ImGui::DockBuilderSplitNode(
        dock_main,
        ImGuiDir_Left,
        0.18f,
        &dock_left,
        &dock_main
    );

    ImGui::DockBuilderSplitNode(
        dock_main,
        ImGuiDir_Right,
        0.23f,
        &dock_right,
        &dock_main
    );

    ImGui::DockBuilderDockWindow(
        "Stats###stats-window",
        dock_left
    );

    ImGui::DockBuilderDockWindow(
        "Settings###settings-window",
        dock_right
    );

    ImGui::DockBuilderDockWindow(
        "Node Details###node-details-window",
        dock_right
    );

    ImGui::DockBuilderDockWindow(
        "KNS Intelligence",
        dock_right
    );

    ImGui::DockBuilderDockWindow(
        "Network###network-window",
        dock_main
    );
    ImGui::DockBuilderDockWindow("Topology###topology-window", dock_right);
    ImGui::DockBuilderDockWindow("TCP Connections###tcp-connections-window", dock_right);
    ImGui::DockBuilderDockWindow("TCP Sessions###tcp-sessions-window", dock_left);
    ImGui::DockBuilderDockWindow("Event Log###event-log-window", dock_left);
    ImGui::DockBuilderDockWindow("TCP Congestion Control###tcp-congestion-window", dock_left);

    ImGui::DockBuilderFinish(
        dockspace_id
    );
}

static void BeginDockSpaceHost(
    bool& dock_initialized
)
{
    ImGuiWindowFlags dockspace_flags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport =
        ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(
        viewport->WorkPos
    );

    ImGui::SetNextWindowSize(
        viewport->WorkSize
    );

    ImGui::SetNextWindowViewport(
        viewport->ID
    );

    dockspace_flags |=
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowRounding,
        0.0f
    );

    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowBorderSize,
        0.0f
    );

    ImGui::Begin(
        "DockSpace Host",
        nullptr,
        dockspace_flags
    );

    ImGui::PopStyleVar(2);

    const ImGuiID dockspace_id =
        ImGui::GetID("MainDockSpace");

    ImGui::DockSpace(
        dockspace_id,
        ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

    if (!dock_initialized)
    {
        SetupDockingLayout();
        dock_initialized = true;
    }

    ImGui::End();
}

static void renderConfigWindow(
    TranslationService& translations,
    const std::string& topologyPath,
    bool& watchTopology,
    const std::string& liveError,
    const std::string& fileStatus,
    bool& loadRequested,
    bool& saveRequested,
    bool canSaveToHub,
    bool& saveHubRequested
)
{
    const std::string window_label =
        translations.label("Settings", "settings-window");
    ImGui::Begin(window_label.c_str());

    ImGui::Text(
        "%s",
        translations.translate("Load a JSON Topology.").c_str()
    );

    ImGui::Separator();

    const std::string load_label =
        translations.label("Load Topology", "load-topology");
    if (ImGui::Button(load_label.c_str()))
    {
        loadRequested = true;
    }

    if (ImGui::Button(translations.label("Save Topology As...", "save-topology").c_str())) {
        saveRequested = true;
    }
    ImGui::BeginDisabled(!canSaveToHub);
    if (ImGui::Button(translations.label("Save to Topology Hub", "save-topology-hub").c_str())) {
        saveHubRequested = true;
    }
    ImGui::EndDisabled();
    if (!fileStatus.empty()) ImGui::TextWrapped("%s", fileStatus.c_str());

    ImGui::BeginDisabled(topologyPath.empty());
    ImGui::Checkbox(translations.label("Follow topology file", "follow-topology").c_str(), &watchTopology);
    ImGui::EndDisabled();
    if (!topologyPath.empty()) ImGui::TextWrapped("Source: %s", topologyPath.c_str());
    if (watchTopology) ImGui::TextWrapped("File updates replace the graph configuration, including manual edits. Simulation time and sessions are preserved.");
    if (!liveError.empty()) ImGui::TextWrapped("Live update: %s", liveError.c_str());
    ImGui::End();
}

static void exportNetworkAnalysis(
    const kns::Topology& topology
)
{
    if (topology.size() <= 0) {
        return;
    }

    try {
        const kns::analysis::NetworkAnalyzer analyzer;

        const auto analysis =
            analyzer.analyze(topology);

        // ============================================
        // Build intelligence request
        // ============================================

        const auto intelligenceRequest =
            kns::intelligence::
                IntelligenceRequestBuilder::build(
                    analysis,
                    kns::intelligence::
                        AnalysisMode::Detailed
                );

        // ============================================
        // Output directory
        // ============================================

        const auto outputDirectory =
            std::filesystem::current_path() /
            "results";

        std::filesystem::create_directories(
            outputDirectory
        );

        // ============================================
        // Full deterministic analysis
        // ============================================

        const auto analysisJson =
            kns::analysis::
                AnalysisJsonSerializer::toJson(
                    analysis
                );

        {
            std::ofstream output(
                outputDirectory /
                "network_analysis.json"
            );

            if (output.is_open()) {
                output
                    << analysisJson.dump(4);
            }
        }

        // ============================================
        // Compact AI context
        // ============================================

        const auto aiContext =
            kns::analysis::
                AIContextBuilder::build(
                    analysis
                );

        {
            std::ofstream output(
                outputDirectory /
                "ai_context.json"
            );

            if (output.is_open()) {
                output
                    << aiContext.dump(4);
            }
        }

        // ============================================
        // Intelligence request
        // ============================================

        const auto requestJson =
            kns::intelligence::
                IntelligenceRequestBuilder::toJson(
                    intelligenceRequest
                );

        {
            std::ofstream output(
                outputDirectory /
                "intelligence_request.json"
            );

            if (output.is_open()) {
                output
                    << requestJson.dump(4);
            }
        }

        std::cout
            << "[ANALYSIS] Export completed\n";
    }
    catch (const std::exception& e) {
        std::cerr
            << "[ANALYSIS] Export failed: "
            << e.what()
            << '\n';
    }
}

static std::optional<
    kns::analysis::NetworkAnalysis
>
analyzeTopology(
    const kns::Topology& topology
)
{
    if (topology.size() <= 0) {
        return std::nullopt;
    }

    const kns::analysis::NetworkAnalyzer analyzer;

    return analyzer.analyze(
        topology
    );
}

static void visualizeWindow(
    std::unique_ptr<SimulationEngine>& engine,
    Topology& topo,
    SimulationState& state,
    GLFWwindow* window,
    CircularBuffer& buffer,
    int& packetSize,
    RunConfig runConfig,
    std::string topologyPath,
    bool watchTopology,
    std::optional<kns::app::hub::HubTopology> hubDocument
)
{
    if (!engine) {
        engine =
            std::make_unique<
                SimulationEngine
            >(topo);
    }

    VisualPacketManager visualManager;
    TranslationService translations;

    float lossProb = 0.0f;
    bool lossOverride = false;
    float speedMultiplier = 1.0f;

    EventLog eventLog;

    auto configureEngine =
        [&](std::unique_ptr<SimulationEngine>& eng)
    {
        RunConfig config = runConfig;
        config.packet_size = packetSize;
        eng->configureRun(config);

        if (lossOverride) eng->setGlobalLossProb(lossProb);

        eng->setLatencyObserver(
            [&buffer](double lat)
            {
                buffer.addLatencyToBuffer(
                    static_cast<float>(lat)
                );
            }
        );

        eng->setPacketObserver(
            [&visualManager, &eventLog](
                const Packet& p,
                std::uint64_t session_id,
                int from,
                int to,
                double departureTime,
                double arrivalTime
            )
            {
                visualManager.observePacket(
                    p,
                    session_id,
                    from,
                    to,
                    departureTime,
                    arrivalTime
                );

                std::ostringstream oss;

                oss << std::fixed
                    << std::setprecision(3)
                    << departureTime
                    << " "
                    << from
                    << " -> "
                    << to
                    << " session="
                    << session_id;

                eventLog.add(
                    departureTime,
                    p.packet_type,
                    from,
                    to,
                    session_id,
                    oss.str()
                );
            }
        );
    };

    configureEngine(engine);

    // --------------------------------------------------
    // Optional environment-based auto start
    // --------------------------------------------------

    const bool gui_auto_start =
        autoStartFromEnvironment(false);

    if (gui_auto_start) {
        generatePackets(engine, topo);
    }

    state =
        engine->hasEvents()
            ? (gui_auto_start ? SimulationState::Running : SimulationState::Paused)
            : SimulationState::Ready;

    double visualTime = 0.0;

    double lastRealTime =
        glfwGetTime();

    int selected_node = -1;

    TcpConnectionPanel tcpConnectionPanel;
    TopologyPanel topologyPanel;
    TopologyCanvas topologyCanvas;

    auto restartSimulation = [&]()
    {
        topo = engine->getTopology().cloneForRun();
        visualTime = 0.0;
        lastRealTime = glfwGetTime();
        visualManager.clear();
        eventLog.clear();
        buffer.clear();

        engine = std::make_unique<SimulationEngine>(topo);
        configureEngine(engine);

        state = SimulationState::Ready;
    };

    bool dock_initialized = false;

    std::optional<
        kns::analysis::NetworkAnalysis
    > currentAnalysis;

    std::uint64_t topologyRevision = 0;
    std::uint64_t observedRevision = engine->getTopology().getRoutingRevision();
    LiveTopologyWatcher liveWatcher;
    std::string liveError;
    std::string fileStatus;

    if (topo.size() > 0) {
        currentAnalysis =
            analyzeTopology(topo);
    }

    const auto intelligenceConfig =
        kns::app::intelligence::
            IntelligenceConfigLoader::fromEnvironment();

    kns::app::gui::IntelligencePanel intelligencePanel(
        intelligenceConfig
    );

    while (!glfwWindowShouldClose(window))
    {
        liveWatcher.setSource(topologyPath, watchTopology);
        if (auto snapshot = liveWatcher.poll()) {
            try {
                engine->synchronizeTopology(*snapshot);
                liveError.clear();
            } catch (const std::exception& exception) {
                liveError = exception.what();
            }
        }
        const double currentRealTime =
            glfwGetTime();

        const double deltaRealTime =
            currentRealTime -
            lastRealTime;

        lastRealTime =
            currentRealTime;

        // --------------------------------------------------
        // Run simulation
        // --------------------------------------------------

        if (state == SimulationState::Running)
        {
            visualTime +=
                (
                    deltaRealTime *
                    speedMultiplier
                ) /
                kSimToVisualScale;

            int safetyCounter = 0;

            while (
                engine->hasEvents() &&
                visualTime >=
                    engine->peekNextEventTime() &&
                safetyCounter < 1000
            )
            {
                engine->processEvent();

                ++safetyCounter;
            }

            if (!engine->hasEvents())
            {
                state =
                    SimulationState::Finished;
            }
        }

        visualManager.update(
            visualTime
        );

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        BeginDockSpaceHost(
            dock_initialized
        );

        // --------------------------------------------------
        // Automatic topology dialog
        // --------------------------------------------------

        bool loadRequested = false;
        bool saveRequested = false;
        bool saveHubRequested = false;

        bool stepRequested = false;
        bool restartRequested = false;

        // --------------------------------------------------
        // Statistics / simulation controls
        // --------------------------------------------------

        renderStatsWindow(
            *engine,
            translations,
            state,
            engine->getStats(),
            buffer,
            packetSize,
            lossProb,
            lossOverride,
            speedMultiplier,
            stepRequested,
            engine->hasEvents(),
            restartRequested
        );

        if (restartRequested) {
            restartSimulation();
        }

        // --------------------------------------------------
        // Step
        // --------------------------------------------------

        if (stepRequested &&
            engine->hasEvents())
        {
            engine->processEvent();

            if (!engine->hasEvents())
            {
                state =
                    SimulationState::Finished;
            }
        }

        // --------------------------------------------------
        // Settings
        // --------------------------------------------------

        renderConfigWindow(
            translations,
            topologyPath,
            watchTopology,
            liveWatcher.error().empty() ? liveError : liveWatcher.error(),
            fileStatus,
            loadRequested,
            saveRequested,
            hubDocument.has_value(),
            saveHubRequested
        );

        topologyPanel.render(*engine, translations);

        if (const auto action = tcpConnectionPanel.render(
                *engine,
                translations
            );
            action.has_value())
        {
            if (action->type == TcpConnectionActionType::Open) {
                engine->startTCPConnection(
                    action->source,
                    action->destination,
                    action->source_port,
                    action->destination_port
                );
                state = SimulationState::Paused;
            } else if (engine->cancelTCPSession(action->session_id) &&
                       !engine->hasEvents()) {
                state = SimulationState::Ready;
            }
        }

        // --------------------------------------------------
        // Network interaction
        // --------------------------------------------------

        if (const auto connection = topologyCanvas.render(*engine, selected_node,
                visualManager.getActivePackets(), visualTime, translations)) {
            engine->startTCPConnection(connection->first, connection->second);
            state = SimulationState::Paused;
        }

        renderSelectedNodePanel(
            engine->getTopology(),
            selected_node,
            engine->getRoutingTable(selected_node),
            translations
        );

        // --------------------------------------------------
        // Topology loading
        // --------------------------------------------------

        if (loadRequested)
        {
            try
            {
                const auto path = gui::chooseTopologyFile(window, false,
                    translations.translate("Select file"));
                if (path)
                {
                    topo = TopologyLoader::load_topology(*path);
                    hubDocument.reset();
                    topologyPath = *path;
                    topologyCanvas.reset();
                    // Invalidate pending reads even when reloading the same path.
                    liveWatcher.setSource({}, false);
                    liveError.clear();
                    fileStatus.clear();

                    currentAnalysis =
                        analyzeTopology(topo);

                    ++topologyRevision;

                    exportNetworkAnalysis(topo);

                    visualTime = 0.0;
                    lastRealTime = glfwGetTime();
                    visualManager.clear();

                    engine =
                        std::make_unique<SimulationEngine>(
                            topo
                        );

                    configureEngine(engine);

                    restartSimulation();

                    const bool loaded_auto_start =
                        autoStartFromEnvironment(false);

                    if (loaded_auto_start)
                    {
                        generatePackets(engine, topo);
                    }

                    selected_node = -1;
                    observedRevision = engine->getTopology().getRoutingRevision();

                    state =
                        engine->hasEvents()
                            ? SimulationState::Paused
                            : SimulationState::Ready;
                }
            }
            catch (const std::exception& e)
            {
                fileStatus = translations.translate("Load failed:") + " " + e.what();
                std::cerr << "Load error: " << e.what() << '\n';
            }
            // Native dialogs are modal; their elapsed time is not simulation time.
            lastRealTime = glfwGetTime();
        }

        if (saveRequested) {
            try {
                const auto path = gui::chooseTopologyFile(window, true,
                    translations.translate("Save topology"));
                if (path) {
                    TopologyLoader::save_topology(engine->getTopology(), *path);
                    fileStatus = translations.translate("Topology saved:") + " " + *path;
                }
            } catch (const std::exception& exception) {
                fileStatus = translations.translate("Save failed:") + " " + exception.what();
            }
            lastRealTime = glfwGetTime();
        }

        if (saveHubRequested && hubDocument) {
            try {
                hubDocument->topology = engine->getTopology().cloneForRun();
                auto client = kns::app::hub::TopologyHubClient(
                    topologyHubConfigFromEnvironment()
                );
                *hubDocument = client.saveTopology(*hubDocument);
                topo = hubDocument->topology.cloneForRun();
                fileStatus = translations.translate("Topology saved to Hub. Revision:") +
                    " " + std::to_string(hubDocument->version);
            } catch (const std::exception& exception) {
                fileStatus = translations.translate("Hub save failed:") + " " + exception.what();
            }
            lastRealTime = glfwGetTime();
        }

        renderEventLogWindow(
            eventLog,
            translations
        );

        renderTCPSessionsWindow(
            *engine,
            translations
        );

        // ============================================
        // KNS Intelligence
        // ============================================

        const auto revision = engine->getTopology().getRoutingRevision();
        if (revision != observedRevision) {
            observedRevision = revision;
            ++topologyRevision;
            currentAnalysis = analyzeTopology(engine->getTopology());
        }
        intelligencePanel.render(
            currentAnalysis,
            topologyRevision
        );

        ImGui::Render();

        glClearColor(
            1.0f,
            1.0f,
            1.0f,
            1.0f
        );

        glClear(
            GL_COLOR_BUFFER_BIT
        );

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(
            window
        );
    }
}

static void shutdownWindow(
    GLFWwindow* window
)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char* argv[])
{
    bool headless = false;
    bool watchTopology = false;

    int topologyPathIndex = -1;
    int outputPathIndex = -1;
    std::optional<std::string> hubTopologyId;
    std::optional<RoutingMetric> routingMetric;
    RunConfig runConfig;

    Topology topo;
    std::optional<kns::app::hub::HubTopology> hubDocument;

    // --------------------------------------------------
    // Parse command-line arguments
    // --------------------------------------------------

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg =
            argv[i];

        if (arg == "--headless")
        {
            headless = true;
            continue;
        }

        if (arg == "--help" || arg == "-h")
        {
            printUsage(std::cout);
            return 0;
        }

        if (arg == "--hub-topology")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for --hub-topology\n";
                printUsage(std::cerr);
                return 1;
            }

            hubTopologyId = argv[++i];
            continue;
        }

        if (arg == "--topology" || arg == "--watch-topology")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "Missing value for " << arg << '\n';
                printUsage(std::cerr);

                return 1;
            }

            topologyPathIndex = ++i;
            watchTopology = arg == "--watch-topology";
            continue;
        }

        if (arg == "--output")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "Missing value for --output\n";
                printUsage(std::cerr);

                return 1;
            }

            outputPathIndex = ++i;
            continue;
        }

        if (arg == "--seed" || arg == "--packet-size") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << '\n';
                return 1;
            }
            const std::string_view value = argv[++i];
            std::uint64_t parsed = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || end != value.data() + value.size() ||
                (arg == "--packet-size" && (parsed == 0 || parsed > std::numeric_limits<int>::max()))) {
                std::cerr << "Invalid value for " << arg << ": " << value << '\n';
                return 1;
            }
            if (arg == "--seed") {
                runConfig.seed = parsed;
            } else {
                runConfig.packet_size = static_cast<int>(parsed);
            }
            continue;
        }

        if (arg == "--routing-metric")
        {
            if (i + 1 >= argc)
            {
                std::cerr
                    << "Missing value for --routing-metric\n";
                printUsage(std::cerr);
                return 1;
            }

            const std::string_view value = argv[++i];
            routingMetric = parseRoutingMetric(value);
            if (!routingMetric)
            {
                std::cerr
                    << "Invalid value for --routing-metric: "
                    << value
                    << '\n';
                printUsage(std::cerr);
                return 1;
            }

            continue;
        }

        if (!arg.starts_with("--") &&
            topologyPathIndex == -1)
        {
            topologyPathIndex = i;
            continue;
        }

        std::cerr
            << "Unknown argument: "
            << arg
            << '\n';
        printUsage(std::cerr);

        return 1;
    }

    if (hubTopologyId && topologyPathIndex >= 0) {
        std::cerr << "--hub-topology cannot be combined with a local topology file\n";
        return 1;
    }
    if (hubTopologyId && watchTopology) {
        std::cerr << "--hub-topology cannot be combined with --watch-topology\n";
        return 1;
    }
    if (watchTopology && headless) {
        std::cerr << "--watch-topology requires GUI mode\n";
        return 1;
    }
    if (routingMetric && !headless)
    {
        std::cerr
            << "--routing-metric is only valid with --headless\n";
        printUsage(std::cerr);
        return 1;
    }

    // --------------------------------------------------
    // Headless mode
    // --------------------------------------------------

    if (headless)
    {
        if (!hubTopologyId &&
            (topologyPathIndex < 0 || topologyPathIndex >= argc))
        {
            std::cerr
                << "Missing required --topology or --hub-topology for headless mode\n";
            printUsage(std::cerr);

            return 1;
        }

        try
        {
            if (hubTopologyId) {
                hubDocument = loadTopologyFromHub(*hubTopologyId);
                topo = hubDocument->topology.cloneForRun();
            } else {
                topo = TopologyLoader::load_topology(argv[topologyPathIndex]);
            }

            auto currentAnalysis =
                analyzeTopology(
                    topo
                );
            
            exportNetworkAnalysis(topo);
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Topology load error: "
                << e.what()
                << '\n';

            return 1;
        }

        auto engine =
            std::make_unique<SimulationEngine>(
                topo
            );

        engine->setRoutingMetric(
            routingMetric.value_or(RoutingMetric::Delay)
        );

        engine->configureRun(runConfig);

        const bool headless_auto_start =
            autoStartFromEnvironment(
                runConfig.auto_start
            );

        if (headless_auto_start)
        {
            generatePackets(
                engine,
                topo
            );
        }

        while (engine->hasEvents())
        {
            engine->processEvent();
        }

        if (outputPathIndex >= 0 &&
            outputPathIndex < argc)
        {
            runConfig.filename =
                argv[outputPathIndex];
        }
        else
        {
            runConfig.filename =
                "results/results.csv";
        }

        try {
            const auto dir = fs::path(runConfig.filename).parent_path();
            if (!dir.empty()) {
                fs::create_directories(dir);
            }
            engine->exportStatsCSV(runConfig);
        } catch (const std::exception& error) {
            std::cerr << "CSV export failed for " << runConfig.filename
                      << ": " << error.what() << '\n';
            return 1;
        }

        const ValidationReport report =
            engine->validateSimulation();

        return report.passed()
            ? 0
            : 1;
    }

    // --------------------------------------------------
    // GUI mode
    // --------------------------------------------------

    if (hubTopologyId ||
        (topologyPathIndex >= 0 && topologyPathIndex < argc))
        {
            try {
                if (hubTopologyId) {
                    hubDocument = loadTopologyFromHub(*hubTopologyId);
                    topo = hubDocument->topology.cloneForRun();
                } else {
                    topo = TopologyLoader::load_topology(argv[topologyPathIndex]);
                }

                auto currentAnalysis =
                    analyzeTopology(
                        topo
                    );

                exportNetworkAnalysis(topo);
            }
            catch (const std::exception& e) {
                std::cerr
                    << "Topology load error: "
                    << e.what()
                    << '\n';

                return 1;
            }
        }

    SimulationState state =
        SimulationState::Ready;

    auto engine =
        std::make_unique<SimulationEngine>(
            topo
        );

    CircularBuffer buffer;

    int packetSize = runConfig.packet_size;

    Window windowMethods;

    GLFWwindow* window =
        windowMethods.generate_window();

    if (!window)
    {
        return 1;
    }

    ImGuiIO& io =
        ImGui::GetIO();

    io.ConfigFlags |=
        ImGuiConfigFlags_DockingEnable;

    visualizeWindow(
        engine,
        topo,
        state,
        window,
        buffer,
        packetSize,
        runConfig,
        topologyPathIndex >= 0 ? argv[topologyPathIndex] : "",
        watchTopology,
        std::move(hubDocument)
    );

    shutdownWindow(
        window
    );

    engine->validateSimulation();

    return 0;
}
