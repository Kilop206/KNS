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

#include "engine/core/SimulationEngine.hpp"
#include "engine/core/SimulationState.hpp"
#include "engine/core/Stats.hpp"
#include "gui/include/LatencyChart.hpp"
#include "gui/include/MetricsPannel.hpp"
#include "gui/include/PacketRenderer.hpp"
#include "gui/include/VisualPacketManager.hpp"
#include "gui/include/VisualPacket.hpp"
#include "gui/include/Window.hpp"
#include "network/Packet.hpp"
#include "network/Routing.hpp"
#include "network/Topology.hpp"
#include "network/TopologyLoader.hpp"

using namespace kns;
using namespace interface;

constexpr double kBasePacketsPerSecond = 1.0;
constexpr double kBasePacketsPerMinute  = kBasePacketsPerSecond * 60.0;
constexpr int    kPacketsPerRoute       = 100;
constexpr double kSimToVisualScale      = 100.0;

struct PickedNodes {
    int origin = -1;
    int dest = -1;
    bool tcp = false;
};

static std::vector<std::pair<int, int>> buildConnectionPlan(const Topology& topo)
{
    std::set<std::pair<int, int>> connections;

    for (std::size_t i = 0; i < static_cast<std::size_t>(topo.size()); ++i) {
        for (const auto& link : topo.getLinksFromNode(static_cast<int>(i))) {
            if (link.from >= 0 && link.to >= 0) {
                connections.insert({link.from, link.to});
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

    std::cout << "[DEBUG] connection count = " << plan.size() << '\n';

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

    if (state == SimulationState::Paused && engineHasEvents) {
        if (ImGui::Button("Step")) {
            stepRequested = true;
        }
    }

    MetricsPannel panel;
    panel.render(stats, buffer);

    if (ImGui::Button(state == SimulationState::Paused ? "Resume" : "Pause")) {
        state = (state == SimulationState::Paused)
            ? SimulationState::Running
            : SimulationState::Paused;
    }

    if (ImGui::SliderFloat("Loss Probability", &lossProb, 0.0f, 1.0f)) {
        engine->setGlobalLossProb(lossProb);
    }

    if (ImGui::SliderInt("Packet Size (bytes)", &packetSize, 100, 10'000)) {
        engine->setGlobalPacketSize(packetSize);
    }

    ImGui::SliderFloat("Simulation speed", &speedMultiplier, 0.25f, 4.0f, "%.2fx");

    ImGui::Separator();
    ImGui::Text("Network configuration:");
    ImGui::Text("Base rate: %.0f packets/min at 1.0x", kBasePacketsPerMinute);
    ImGui::Text("Packets per route: %d", kPacketsPerRoute);

    if (!engineHasEvents) {
        ImGui::Separator();
        ImGui::TextDisabled("Simulation finished.");
    }

    ImGui::End();
}

static void drawLinks(
    ImDrawList* draw_list,
    const Topology& topo,
    const std::vector<std::pair<float, float>>& positions
) {
    for (std::size_t i = 0; i < static_cast<std::size_t>(topo.size()); ++i) {
        const auto& links = topo.getLinksFromNode(static_cast<int>(i));
        for (const auto& link : links) {
            if (link.from < 0 || link.to < 0 ||
                link.from >= static_cast<int>(positions.size()) ||
                link.to   >= static_cast<int>(positions.size())) {
                continue;
            }

            ImVec2 p1(positions[link.from].first, positions[link.from].second);
            ImVec2 p2(positions[link.to].first, positions[link.to].second);

            draw_list->AddLine(p1, p2, IM_COL32(255, 255, 0, 255), 2.0f);
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
            ? IM_COL32(255, 255, 0, 255)
            : IM_COL32(100, 200, 100, 255);

        draw_list->AddCircleFilled(
            ImVec2(positions[i].first, positions[i].second),
            10.0f,
            color
        );

        draw_list->AddText(
            ImVec2(positions[i].first + 12.0f, positions[i].second - 6.0f),
            IM_COL32(255, 255, 255, 255),
            std::to_string(i).c_str()
        );
    }
}

static void renderSelectedNodePanel(
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

    if (routingTable.empty()) {
        ImGui::Text("Routing table is empty.");
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
        IM_COL32(20, 20, 20, 255)
    );

    const std::vector<std::pair<float, float>> positions =
        generatePositions(topo, canvas_p0, canvas_sz);

    if (topo.size() > 0) {
        drawLinks(draw_list, topo, positions);
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

    std::cout
        << "visualManager address = "
        << &visualManager
        << '\n';

    visualManager.clear();
    float lossProb = 0.0f;
    float speedMultiplier = 1.0f;

    auto configureEngine = [&](std::unique_ptr<SimulationEngine>& eng) {
        eng->setGlobalPacketSize(packetSize);
        eng->setGlobalLossProb(lossProb);
        eng->setLatencyObserver([&buffer](double lat) {
            buffer.addLatencyToBuffer(static_cast<float>(lat));
        });
        eng->setPacketObserver(
            [&visualManager](
                const Packet& p,
                std::uint64_t session_id,
                int from,
                int to,
                double departureTime,
                double arrivalTime
            ) {
                visualManager.observePacket(
                    p,
                    session_id,
                    from,
                    to,
                    departureTime,
                    arrivalTime
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
            visualTime +=
                deltaRealTime *
                speedMultiplier /
                kSimToVisualScale;

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

        if (engine->hasEvents() == false) {
            state = SimulationState::Paused;
        }

        visualManager.update(visualTime);

        std::cout
            << "visualTime="
            << visualTime
            << '\n';

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
            visualManager.update(visualTime);

            std::cout
                << "visualTime="
                << visualTime
                << '\n';
        }

        renderConfigWindow(
            firstFrame,
            topo.size() > 0
        );

        visualManager.update(visualTime);

        std::cout
            << "visualTime="
            << visualTime
            << '\n';

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
            selected_node,
            routingTable
        );

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