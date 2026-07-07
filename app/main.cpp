#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "ImGuiFileDialog.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <deque>
#include <sstream>
#include <iomanip>

#include "engine/core/SimulationEngine.hpp"
#include "engine/core/SimulationState.hpp"
#include "engine/core/Stats.hpp"
#include "gui/include/LatencyChart.hpp"
#include "gui/include/MetricsPannel.hpp"
#include "gui/include/PacketRenderer.hpp"
#include "gui/include/VisualPacketManager.hpp"
#include "gui/include/VisualPacket.hpp"
#include "gui/include/Window.hpp"
#include "gui/include/GUIFormat.hpp"
#include "gui/include/ThemeManager.hpp"
#include "network/Packet.hpp"
#include "network/Routing.hpp"
#include "network/Topology.hpp"
#include "network/TopologyLoader.hpp"

using namespace kns;
using namespace gui;

constexpr double kBasePacketsPerSecond = 1.0;
constexpr double kBasePacketsPerMinute = kBasePacketsPerSecond * 60.0;
constexpr int    kPacketsPerRoute      = 20;
constexpr double kSimToVisualScale     = 20.0;

struct PickedNodes {
    int origin = -1;
    int dest = -1;
    bool tcp = false;
};

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
};

struct VisualLinkUsage {
    int from = -1;
    int to = -1;
    double until = 0.0;
};

static void renderEventLogWindow(const EventLog& log)
{
    ImGui::Begin("Event Log");

    if (ImGui::BeginTable(
            "EventLogTable",
            6,
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f)))
    {
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("From");
        ImGui::TableSetupColumn("To");
        ImGui::TableSetupColumn("Session");
        ImGui::TableSetupColumn("Details");
        ImGui::TableHeadersRow();

        for (const auto& entry : log.lines) {
            ImGui::TableNextRow();

            ImU32 rowColor = IM_COL32(255,255,255,255);

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
            ImGui::TextUnformatted(PacketRenderer::packetTypeToString(entry.type));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", entry.from);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", entry.to);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%llu",
                static_cast<unsigned long long>(entry.session_id));

            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(entry.text.c_str());
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

static std::vector<std::pair<int, int>> buildConnectionPlan(const Topology& topo)
{
    std::set<std::pair<int, int>> connections;

    for (std::size_t i = 0; i < static_cast<std::size_t>(topo.size()); ++i) {
        for (const auto& link : topo.getLinksFromNode(static_cast<int>(i))) {
            if (link->getA() >= 0 && link->getB() >= 0) {
                connections.insert({link->getA(), link->getB()});
            }
        }
    }

    return {connections.begin(), connections.end()};
}

static void generatePackets(
    std::unique_ptr<SimulationEngine>& engine,
    const Topology& topo
) {
    if (!engine || topo.size() <= 0) {
        return;
    }

    const auto plan = buildConnectionPlan(topo);

    for (const auto& [from, to] : plan) {
        engine->startTCPConnection(from, to);
    }
}

static std::vector<std::pair<float, float>> generatePositions(
    const Topology& topo,
    ImVec2 canvas_origin,
    ImVec2 canvas_size
) {
    std::vector<std::pair<float, float>> positions;
    positions.reserve(static_cast<std::size_t>(topo.size()));

    if (topo.size() <= 0) {
        return positions;
    }

    const float centerX = canvas_origin.x + canvas_size.x * 0.5f;
    const float centerY = canvas_origin.y + canvas_size.y * 0.5f;
    const float radius  = std::max(40.0f, 0.35f * std::min(canvas_size.x, canvas_size.y));

    for (std::size_t i = 0; i < static_cast<std::size_t>(topo.size()); ++i) {
        const float angle =
            2.0f * std::numbers::pi_v<float> *
            static_cast<float>(i) /
            static_cast<float>(topo.size());

        positions.push_back({
            centerX + radius * std::cos(angle),
            centerY + radius * std::sin(angle)
        });
    }

    return positions;
}

static int pickNodeAtMouse(
    const std::vector<std::pair<float, float>>& positions,
    float radius
) {
    const ImVec2 mouse_pos = ImGui::GetMousePos();

    for (std::size_t i = 0; i < positions.size(); ++i) {
        const float dx = mouse_pos.x - positions[i].first;
        const float dy = mouse_pos.y - positions[i].second;
        const float dist2 = dx * dx + dy * dy;

        if (dist2 <= radius * radius) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

static void renderStatsWindow(
    std::unique_ptr<SimulationEngine>& engine,
    SimulationState& state,
    const Stats& stats,
    CircularBuffer& buffer,
    int& packetSize,
    float& lossProb,
    float& speedMultiplier,
    bool& stepRequested,
    bool engineHasEvents
) {
    ImGui::Begin("Stats");

    if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button(state == SimulationState::Paused ? "Resume" : "Pause")) {
            state = (state == SimulationState::Paused)
                ? SimulationState::Running
                : SimulationState::Paused;
        }

        if (state == SimulationState::Paused && engineHasEvents) {
            ImGui::SameLine();
            if (ImGui::Button("Step")) {
                stepRequested = true;
            }
        }

        ImGui::SliderFloat("Simulation speed", &speedMultiplier, 0.25f, 4.0f, "%.2fx");

        if (!engineHasEvents) {
            ImGui::TextDisabled("Simulation finished.");
        }
    }

    if (ImGui::CollapsingHeader("Traffic", ImGuiTreeNodeFlags_DefaultOpen)) {
        float lossPercent = lossProb * 100.0f;

        if (ImGui::SliderFloat("Loss probability", &lossPercent, 0.0f, 100.0f, "%.0f %%")) {
            lossProb = lossPercent / 100.0f;
            engine->setGlobalLossProb(lossProb);
        }

        if (ImGui::SliderInt("Packet size (bytes)", &packetSize, 64, 65535, "%d B")) {
            engine->setGlobalPacketSize(packetSize);
        }

        ImGui::Text("Current size: %s", formatBytes(packetSize).c_str());
    }

    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        MetricsPannel panel;
        panel.render(stats, buffer);
    }

    if (ImGui::CollapsingHeader("Network Configuration")) {
        ImGui::Text("Base rate: %.0f packets/min at 1.0x", kBasePacketsPerMinute);
        ImGui::Text("Packets per route: %d", kPacketsPerRoute);
    }

    if (ImGui::CollapsingHeader("Validation")) {
        ImGui::Text("Packets sent: %d", stats.packets_sent);
        ImGui::Text("Packets delivered: %d", stats.packets_delivered);
        ImGui::Text("Packets lost: %d", stats.packets_lost);
    }

    ImGui::End();
}

static void drawLinks(
    ImDrawList* draw_list,
    const Topology& topo,
    const std::vector<std::pair<float, float>>& positions,
    const std::vector<VisualLinkUsage>& activeLinks
) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(topo.size()); ++i) {
        const auto& links = topo.getLinksFromNode(static_cast<int>(i));

        for (const auto& link : links) {
            const int a = link->getA();
            const int b = link->getB();

            if (a < 0 || b < 0 ||
                a >= static_cast<int>(positions.size()) ||
                b >= static_cast<int>(positions.size())) {
                continue;
            }

            bool occupied = false;
            for (const auto& usage : activeLinks) {
                if ((usage.from == a && usage.to == b) ||
                    (usage.from == b && usage.to == a)) {
                    occupied = true;
                    break;
                }
            }

            const ImU32 color = occupied
                ? IM_COL32(255, 80, 0, 255)
                : IM_COL32(0, 0, 0, 255);

            const float thickness = occupied
                ? 8.0f
                : 2.0f;

            ImVec2 p1(positions[a].first, positions[a].second);
            ImVec2 p2(positions[b].first, positions[b].second);

            draw_list->AddLine(p1, p2, color, thickness);
        }
    }
}

static void drawNodes(
    ImDrawList* draw_list,
    const Topology& topo,
    const std::vector<std::pair<float, float>>& positions,
    int selected_node
) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(topo.size()); ++i) {
        const ImU32 color = (static_cast<int>(i) == selected_node)
            ? IM_COL32(128, 128, 128, 255)
            : IM_COL32(169, 169, 169, 255);

        const float radius = 20.0f;
        const std::string label = std::to_string(i);
        const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());

        const float x = positions[i].first;
        const float y = positions[i].second;

        draw_list->AddCircleFilled(
            ImVec2(x, y),
            radius,
            color
        );

        draw_list->AddText(
            ImVec2(
                x - text_size.x * 0.5f,
                y - text_size.y * 0.55f
            ),
            IM_COL32(255, 255, 255, 255),
            label.c_str()
        );
    }
}

static void renderSelectedNodePanel(
    const Topology& topo,
    int selected_node,
    const std::vector<Routing::RoutingEntry>& routingTable
) {
    ImGui::Begin("Node Details");

    if (selected_node < 0) {
        ImGui::Text("No node selected.");
        ImGui::End();
        return;
    }

    ImGui::Text("Selected node: %d", selected_node);
    ImGui::Separator();

    if (selected_node >= topo.size()) {
        ImGui::Text("Invalid node.");
        ImGui::End();
        return;
    }

    ImGui::Text("Neighbors:");
    const auto& links = topo.getLinksFromNode(selected_node);

    bool hasNeighbors = false;
    for (const auto& link : links) {
        const int other = link->getOtherNode(selected_node);
        if (other != -1) {
            hasNeighbors = true;
            ImGui::BulletText("%d", other);
        }
    }

    if (!hasNeighbors) {
        ImGui::TextDisabled("No neighbors.");
    }

    ImGui::Separator();
    ImGui::Text("Routing table:");

    if (routingTable.empty()) {
        ImGui::TextDisabled("Routing table is empty.");
    } else {
        for (const auto& entry : routingTable) {
            if (entry.distance == std::numeric_limits<double>::infinity()) {
                ImGui::Text("Dest: %d | Next: - | Dist: inf", entry.destination);
            } else {
                ImGui::Text(
                    "Dest: %d | Next: %d | Dist: %.2f",
                    entry.destination,
                    entry.next_hop,
                    entry.distance
                );
            }
        }
    }

    ImGui::End();
}

static void SetupDockingLayout() {
    const ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    ImGuiID dock_main  = dockspace_id;
    ImGuiID dock_left  = 0;
    ImGuiID dock_right = 0;

    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.25f, &dock_left, &dock_main);
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.28f, &dock_right, &dock_main);

    ImGui::DockBuilderDockWindow("Stats",        dock_left);
    ImGui::DockBuilderDockWindow("Settings",     dock_right);
    ImGui::DockBuilderDockWindow("Node Details", dock_right);
    ImGui::DockBuilderDockWindow("Network",      dock_main);

    ImGui::DockBuilderFinish(dockspace_id);
}

static void BeginDockSpaceHost(bool& dock_initialized) {
    ImGuiWindowFlags dockspace_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    dockspace_flags |= ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("DockSpace Host", nullptr, dockspace_flags);
    ImGui::PopStyleVar(2);

    const ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!dock_initialized) {
        SetupDockingLayout();
        dock_initialized = true;
    }

    ImGui::End();
}


static PickedNodes renderNetworkPanel(
    const Topology& topo,
    int selected_node,
    const std::vector<VisualPacket>& visualPackets,
    double visualTime
) {
    static int drag_source_node = -1;

    ImGui::Begin("Network");

    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

    if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
    if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(
        canvas_p0,
        ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y),
        true
    );

    draw_list->AddRectFilled(
        canvas_p0,
        ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y),
        IM_COL32(245, 245, 245, 255)
    );

    const std::vector<std::pair<float, float>> positions =
        generatePositions(topo, canvas_p0, canvas_sz);

    int hovered_node = -1;
    if (ImGui::IsWindowHovered()) {
        hovered_node = pickNodeAtMouse(positions, 20.0f);
    }

    if (hovered_node != -1) {
        ImGui::BeginTooltip();
        ImGui::Text("Node %d", hovered_node);

        if (hovered_node < topo.size()) {
            const auto& links = topo.getLinksFromNode(hovered_node);

            int neighbors = 0;
            for (const auto& link : links) {
                if (link->getOtherNode(hovered_node) != -1) {
                    ++neighbors;
                }
            }

            ImGui::Text("Neighbors: %d", neighbors);
            ImGui::Text("Click to inspect");
        }

        ImGui::EndTooltip();
    }

    if (topo.size() > 0) {
        std::vector<VisualLinkUsage> activeLinks;

        for (const auto& packet : visualPackets)
        {
            VisualLinkUsage usage;

            usage.from = packet.from;
            usage.to = packet.to;
            usage.until = packet.sim_arrival_time;

            activeLinks.push_back(usage);
        }

        drawLinks(draw_list, topo, positions, activeLinks);
        drawNodes(draw_list, topo, positions, selected_node);

        PacketRenderer packetRenderer;
        packetRenderer.render(
            draw_list,
            positions,
            visualPackets,
            visualTime
        );
    }

    ImGui::InvisibleButton("network_canvas", canvas_sz);
    const bool hovered = ImGui::IsItemHovered();

    int clicked_node = -1;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const int node = pickNodeAtMouse(positions, 10.0f);
        if (node != -1) {
            clicked_node = node;
            drag_source_node = node;
        }
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && drag_source_node != -1) {
        ImVec2 src(positions[drag_source_node].first, positions[drag_source_node].second);
        ImVec2 mouse = ImGui::GetMousePos();
        draw_list->AddLine(src, mouse, IM_COL32(255, 255, 255, 150), 1.5f);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && drag_source_node != -1) {
        const int dest = pickNodeAtMouse(positions, 10.0f);
        if (dest != -1 && dest != drag_source_node) {
            const int src = drag_source_node;
            drag_source_node = -1;

            draw_list->PopClipRect();
            ImGui::End();
            return PickedNodes{src, dest, true};
        }
        drag_source_node = -1;
    }

    PacketRenderer legendRenderer;
    const float legend_width = 180.0f;
    const float legend_line_h = 18.0f;
    const float legend_padding = 10.0f;
    const float legend_height = 20.0f + (5.0f * legend_line_h) + (2.0f * legend_padding);

    ImVec2 legend_p0(
        canvas_p0.x + canvas_sz.x - legend_width - 10.0f,
        canvas_p0.y + 10.0f
    );
    ImVec2 legend_p1(
        legend_p0.x + legend_width,
        legend_p0.y + legend_height
    );

    draw_list->AddRectFilled(legend_p0, legend_p1, IM_COL32(32, 32, 32, 220), 8.0f);
    draw_list->AddRect(legend_p0, legend_p1, IM_COL32(255, 255, 255, 70), 8.0f, 0, 1.0f);

    const char* title = "Packet Subtitle";
    const ImVec2 title_sz = ImGui::CalcTextSize(title);
    const float title_y = legend_p0.y + 10.0f + (16.0f - title_sz.y) * 0.5f;

    draw_list->AddText(
        ImVec2(legend_p0.x + 12.0f, title_y),
        IM_COL32(255, 255, 255, 255),
        title
    );

    const float rows_y = legend_p0.y + 34.0f;
    const float row_gap = legend_line_h;
    const float box_size = 12.0f;

    auto addLegendRow = [&](float y, const char* label, ImU32 color)
    {
        const ImVec2 box_p(legend_p0.x + 12.0f, y);

        const ImVec2 text_size = ImGui::CalcTextSize(label);

        const float text_x = box_p.x + box_size + 8.0f;
        const float text_y = box_p.y + (box_size - text_size.y) * 0.5f;

        draw_list->AddRectFilled(
            box_p,
            ImVec2(box_p.x + box_size, box_p.y + box_size),
            color,
            2.0f
        );

        draw_list->AddText(
            ImVec2(text_x, text_y),
            IM_COL32(255,255,255,235),
            label
        );
    };

    addLegendRow(rows_y + row_gap * 0.0f, "SYN",     legendRenderer.packetColorByType(PacketType::SYN));
    addLegendRow(rows_y + row_gap * 1.0f, "SYN-ACK", legendRenderer.packetColorByType(PacketType::SYN_ACK));
    addLegendRow(rows_y + row_gap * 2.0f, "ACK",     legendRenderer.packetColorByType(PacketType::ACK));
    addLegendRow(rows_y + row_gap * 3.0f, "DATA",    legendRenderer.packetColorByType(PacketType::DATA));
    addLegendRow(rows_y + row_gap * 4.0f, "FIN",     legendRenderer.packetColorByType(PacketType::FIN));

    draw_list->PopClipRect();
    ImGui::End();

    return PickedNodes{clicked_node, -1, false};
}

static void renderConfigWindow(bool& firstFrame, bool topologySelected) {
    bool autoClick = false;

    ImGui::Begin("Settings");

    if (firstFrame && !topologySelected) {
        autoClick = true;
        firstFrame = false;
    }

    ImGui::Text("Load a JSON Topology.");
    ImGui::Separator();

    if (ImGui::Button("Load Topology") || autoClick) {
        if (!ImGuiFileDialog::Instance()->IsOpened("TopologyKey")) {
            ImGuiFileDialog::Instance()->OpenDialog(
                "TopologyKey",
                "Select File",
                ".json"
            );
        }
    }

    ImGui::End();
}

static void visualizeWindow(
    std::unique_ptr<SimulationEngine>& engine,
    Topology& topo,
    SimulationState& state,
    GLFWwindow* window,
    CircularBuffer& buffer,
    int& packetSize
) {
    if (!engine) {
        engine = std::make_unique<SimulationEngine>(topo);
    }

    VisualPacketManager visualManager;
    float lossProb = 0.0f;
    float speedMultiplier = 1.0f;

    EventLog eventLog;

    auto configureEngine = [&](std::unique_ptr<SimulationEngine>& eng) {

        eng->setGlobalPacketSize(packetSize);
        eng->setGlobalLossProb(lossProb);

        eng->setLatencyObserver([&buffer](double lat) {
            buffer.addLatencyToBuffer(
                static_cast<float>(lat)
            );
        });

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
                oss << std::fixed << std::setprecision(3)
                    << departureTime << "  "
                    << from << " -> " << to
                    << "  session=" << session_id;

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
    generatePackets(engine, topo);

    double visualTime = 0.0;
    double lastRealTime = glfwGetTime();

    int selected_node = -1;
    std::vector<Routing::RoutingEntry> routingTable;
    Routing routing;

    bool firstFrame = true;
    bool dock_initialized = false;

    while (!glfwWindowShouldClose(window))
    {
        const double currentRealTime = glfwGetTime();
        const double deltaRealTime = currentRealTime - lastRealTime;
        lastRealTime = currentRealTime;

        if (state == SimulationState::Running) {
            visualTime += (deltaRealTime * speedMultiplier) / kSimToVisualScale;

            int safetyCounter = 0;
            while (
                engine->hasEvents() &&
                visualTime >= engine->peekNextEventTime() &&
                safetyCounter < 1000
            ) {
                engine->processEvent();
                ++safetyCounter;
            }
        }

        if (!engine->hasEvents()) {
            state = SimulationState::Paused;
        }

        visualManager.update(visualTime);

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        BeginDockSpaceHost(dock_initialized);

        if (firstFrame && topo.size() == 0)
        {
            ImGuiFileDialog::Instance()->OpenDialog(
                "TopologyKey",
                "Select Initial Topology",
                ".json"
            );

            firstFrame = false;
        }

        bool stepRequested = false;

        renderStatsWindow(
            engine,
            state,
            engine->getStats(),
            buffer,
            packetSize,
            lossProb,
            speedMultiplier,
            stepRequested,
            engine->hasEvents()
        );

        if (stepRequested && engine->hasEvents()) {
            engine->processEvent();
        }

        renderConfigWindow(
            firstFrame,
            topo.size() > 0
        );

        PickedNodes clicked_node = renderNetworkPanel(
            topo,
            selected_node,
            visualManager.getActivePackets(),
            visualTime
        );

        if (clicked_node.tcp)
        {
            engine->startTCPConnection(
                clicked_node.origin,
                clicked_node.dest
            );
        }
        else if (clicked_node.origin != -1)
        {
            selected_node = clicked_node.origin;

            routingTable =
                routing.buildRoutingTable(
                    topo,
                    selected_node
                );
        }

        renderSelectedNodePanel(
            topo,
            selected_node,
            routingTable
        );

        renderEventLogWindow(eventLog);

        if (ImGuiFileDialog::Instance()->Display(
                "TopologyKey",
                ImGuiWindowFlags_NoCollapse,
                ImVec2(400, 300)))
        {
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                try
                {
                    topo = TopologyLoader::load_topology(
                        ImGuiFileDialog::Instance()
                            ->GetFilePathName()
                    );

                    visualTime = 0.0;
                    lastRealTime = glfwGetTime();
                    visualManager.clear();

                    engine =
                        std::make_unique<SimulationEngine>(
                            topo
                        );

                    configureEngine(engine);
                    generatePackets(engine, topo);

                    selected_node = -1;
                    routingTable.clear();

                    state =
                        SimulationState::Paused;
                }
                catch (const std::exception& e)
                {
                    std::cerr
                        << "Load error: "
                        << e.what()
                        << '\n';
                }
            }

            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::Render();

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(
            ImGui::GetDrawData()
        );

        glfwSwapBuffers(window);
    }
}

static void shutdownWindow(GLFWwindow* window) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main(int argc, char* argv[]) {
    Topology topo;

    if (argc >= 2) {
        try {
            topo = TopologyLoader::load_topology(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "Topology load error: " << e.what() << std::endl;
            return -1;
        }
    }

    SimulationState state = SimulationState::Paused;

    auto engine = std::make_unique<SimulationEngine>(topo);
    CircularBuffer buffer;

    int packetSize = 1000;

    Window windowMethods;
    GLFWwindow* window = windowMethods.generate_window();
    if (!window) {
        return -1;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    visualizeWindow(
        engine,
        topo,
        state,
        window,
        buffer,
        packetSize
    );

    shutdownWindow(window);
    engine->validateSimulation();
    return 0;
}