#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "ImGuiFileDialog.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/SimulationEngine.hpp"
#include "engine/core/SimulationState.hpp"
#include "engine/core/Stats.hpp"
#include "engine/events/PacketGenerationEvent.hpp"
#include "engine/events/TCPHandshakeEvent.hpp"
#include "gui/include/LatencyChart.hpp"
#include "gui/include/MetricsPannel.hpp"
#include "gui/include/Window.hpp"
#include "network/Packet.hpp"
#include "network/Routing.hpp"
#include "network/Topology.hpp"
#include "network/TopologyLoader.hpp"
#include "enums/PacketType.hpp"

using namespace kns;
using namespace interface;

constexpr double kBasePacketsPerSecond = 5.0;
constexpr double kBasePacketsPerMinute  = kBasePacketsPerSecond * 60.0;

constexpr int    kPacketsPerRoute   = 250;
constexpr int    kMaxTotalPackets    = 1000;
constexpr double kVisualTravelTime   = 1.2;
constexpr double kVisualSpawnGap     = 0.12;

// ============================================================
// HELPER STRUCTS
// ============================================================

struct PacketSpec {
    int from = -1;
    int to   = -1;
};

struct VisualPacket {
    int from = -1;
    int to   = -1;
    double sim_start_time;
    double sim_end_time;
    PacketType type = PacketType::DATA;
};

// ============================================================
// HELPER FUNCTIONS
// ============================================================

static std::vector<PacketSpec> buildOrderedPacketPlan(const Topology& topo) {
    std::vector<PacketSpec> plan;
    plan.reserve(kMaxTotalPackets);

    std::vector<PacketSpec> routes;
    for (int i = 0; i < topo.size(); ++i) {
        for (const auto& link : topo.getLinksFromNode(i)) {
            if (link.from >= 0 && link.to >= 0) {
                routes.push_back(PacketSpec{link.from, link.to});
            }
        }
    }

    if (routes.empty()) {
        return plan;
    }

    for (int p = 0; p < kPacketsPerRoute; ++p) {
        for (const auto& route : routes) {
            if (static_cast<int>(plan.size()) >= kMaxTotalPackets) {
                return plan;
            }
            plan.push_back(route);
        }
    }

    return plan;
}

static void generatePackets(SimulationEngine& engine, const Topology& topo, double startTime = 1.0) {
    if (topo.size() <= 0) {
        return;
    }

    const auto plan = buildOrderedPacketPlan(topo);
    if (plan.empty()) {
        return;
    }

    const double generationInterval = 1.0 / kBasePacketsPerSecond;

    for (std::size_t i = 0; i < plan.size(); ++i) {
        engine.schedule(std::make_unique<PacketGenerationEvent>(
            startTime + static_cast<double>(i) * generationInterval,
            plan[i].from,
            plan[i].to,
            PacketType::DATA
        ));
    }
}

static void scheduleDemoTraffic(SimulationEngine& engine, const Topology& topo) {
    if (topo.size() >= 2) {
        engine.schedule(std::make_unique<TCPHandshakeEvent>(0.0, 0, 1));
    }

    generatePackets(engine, topo, 1.0);
}

static std::vector<std::pair<float, float>> generatePositions(
    const Topology& topo,
    ImVec2 canvas_origin,
    ImVec2 canvas_size
) {
    std::vector<std::pair<float, float>> positions;
    positions.reserve(topo.size());

    if (topo.size() <= 0) {
        return positions;
    }

    const float centerX = canvas_origin.x + canvas_size.x * 0.5f;
    const float centerY = canvas_origin.y + canvas_size.y * 0.5f;
    const float radius  = std::max(40.0f, 0.35f * std::min(canvas_size.x, canvas_size.y));

    for (int i = 0; i < topo.size(); ++i) {
        const float angle = 2.0f * std::numbers::pi_v<float> * i / topo.size();
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

    for (int i = 0; i < static_cast<int>(positions.size()); ++i) {
        const float dx    = mouse_pos.x - positions[i].first;
        const float dy    = mouse_pos.y - positions[i].second;
        const float dist2 = dx * dx + dy * dy;

        if (dist2 <= radius * radius) {
            return i;
        }
    }

    return -1;
}

static void renderStatsWindow(
    SimulationEngine& engine,
    SimulationState&  state,
    const Stats&      stats,
    CircularBuffer&   buffer,
    int&              packetSize,
    float&            lossProb,
    float&            speedMultiplier,
    bool&             stepRequested,
    bool              engineHasEvents
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
        engine.setGlobalLossProb(lossProb);
    }

    if (ImGui::SliderInt("Packet Size (bytes)", &packetSize, 100, 10'000)) {
        engine.setGlobalPacketSize(packetSize);
    }

    ImGui::SliderFloat("Simulation speed", &speedMultiplier, 0.25f, 4.0f, "%.2fx");

    ImGui::Separator();
    ImGui::Text("Network configuration:");
    ImGui::Text("Base rate: %.0f packets/min at 1.0x", kBasePacketsPerMinute);
    ImGui::Text("Packets per route: %d", kPacketsPerRoute);
    ImGui::Text("Max total packets limit: %d", kMaxTotalPackets);

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
    for (int i = 0; i < topo.size(); ++i) {
        const auto& links = topo.getLinksFromNode(i);
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
    for (int i = 0; i < topo.size(); ++i) {
        ImU32 color = (i == selected_node)
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

static void drawPackets(
    ImDrawList* draw_list,
    const std::vector<std::pair<float, float>>& positions,
    double visualTime,
    std::vector<VisualPacket>& activePackets
) {
    activePackets.erase(
        std::remove_if(
            activePackets.begin(),
            activePackets.end(),
            [visualTime](const VisualPacket& p) {
                return (visualTime - p.sim_end_time) >= kVisualTravelTime;
            }
        ),
        activePackets.end()
    );

    for (const auto& p : activePackets) {
        if (p.from < 0 || p.to < 0 ||
            p.from >= static_cast<int>(positions.size()) ||
            p.to   >= static_cast<int>(positions.size())) {
            continue;
        }

        const double elapsed = visualTime - p.sim_start_time;
        const float t = std::clamp(static_cast<float>(elapsed / kVisualTravelTime), 0.0f, 1.0f);

        const ImVec2 p1(positions[p.from].first, positions[p.from].second);
        const ImVec2 p2(positions[p.to].first, positions[p.to].second);

        const ImVec2 pos(
            p1.x + (p2.x - p1.x) * t,
            p1.y + (p2.y - p1.y) * t
        );

        switch (p.type) {
            case PacketType::SYN:
                draw_list->AddCircleFilled(pos, 6.0f, IM_COL32(0, 150, 255, 255));
                draw_list->AddCircle(pos, 9.0f, IM_COL32(255, 255, 255, 255), 18, 1.5f);
                break;

            case PacketType::SYN_ACK:
                draw_list->AddCircleFilled(pos, 6.0f, IM_COL32(180, 0, 255, 255));
                draw_list->AddCircle(pos, 9.0f, IM_COL32(255, 255, 255, 255), 18, 1.5f);
                draw_list->AddLine(
                    ImVec2(pos.x - 4.0f, pos.y - 4.0f),
                    ImVec2(pos.x + 4.0f, pos.y + 4.0f),
                    IM_COL32(255, 255, 255, 255),
                    1.5f
                );
                draw_list->AddLine(
                    ImVec2(pos.x + 4.0f, pos.y - 4.0f),
                    ImVec2(pos.x - 4.0f, pos.y + 4.0f),
                    IM_COL32(255, 255, 255, 255),
                    1.5f
                );
                break;

            case PacketType::ACK:
                draw_list->AddCircleFilled(pos, 4.5f, IM_COL32(0, 255, 120, 255));
                break;

            case PacketType::DATA:
                draw_list->AddRectFilled(
                    ImVec2(pos.x - 4.0f, pos.y - 4.0f),
                    ImVec2(pos.x + 4.0f, pos.y + 4.0f),
                    IM_COL32(255, 255, 255, 255)
                );
                break;

            default:
                draw_list->AddCircleFilled(pos, 4.0f, IM_COL32(200, 200, 200, 255));
                break;
        }
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
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

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

    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!dock_initialized) {
        SetupDockingLayout();
        dock_initialized = true;
    }

    ImGui::End();
}

static int renderNetworkPanel(
    const Topology& topo,
    int selected_node,
    double visualTime,
    std::vector<VisualPacket>& activePackets
) {
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

    std::vector<std::pair<float, float>> positions =
        generatePositions(topo, canvas_p0, canvas_sz);

    if (topo.size() > 0) {
        drawLinks(draw_list, topo, positions);
        drawNodes(draw_list, topo, positions, selected_node);
        drawPackets(draw_list, positions, visualTime, activePackets);
    }

    ImGui::InvisibleButton("network_canvas", canvas_sz);
    const bool hovered = ImGui::IsItemHovered();

    int clicked_node = -1;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        clicked_node = pickNodeAtMouse(positions, 10.0f);
    }

    draw_list->PopClipRect();
    ImGui::End();

    return clicked_node;
}

static void renderConfigWindow() {
    ImGui::Begin("Settings");

    ImGui::Text("Load a JSON Topology.");
    ImGui::Separator();

    if (ImGui::Button("Load Topology")) {
        ImGuiFileDialog::Instance()->OpenDialog("TopologyKey", "Select File", ".json");
    }

    ImGui::End();
}

static void registerPacketObserver(
    SimulationEngine& engine,
    std::vector<VisualPacket>& activePackets,
    double& visualTime,
    double& arrival
) {
    engine.setPacketObserver(
        [&activePackets, &visualTime](const Packet& p, int from, int to, double now, double arrival) {
            activePackets.push_back(VisualPacket{
                from,
                to,
                now,
                arrival,
                p.packet_type
            });
        }
    );
}

static void visualizeWindow(
    SimulationEngine& engine,
    Topology&         topo,
    SimulationState&  state,
    GLFWwindow*       window,
    CircularBuffer&   buffer,
    int&              packetSize
) {
    static std::vector<VisualPacket> activePackets;
    static double visualTime = 0.0;

    registerPacketObserver(engine, activePackets, visualTime, engine.compute_arrival_ti);

    int selected_node = -1;
    std::vector<Routing::RoutingEntry> routingTable;
    Routing routing;

    static bool firstFrame       = true;
    static bool dock_initialized  = false;
    static double lastRealTime    = glfwGetTime();
    static double simBudget       = 0.0;

    float lossProb        = 0.0f;
    float speedMultiplier = 1.0f;

    while (!glfwWindowShouldClose(window)) {
        const double currentRealTime = glfwGetTime();
        const double deltaRealTime   = currentRealTime - lastRealTime;
        lastRealTime = currentRealTime;

        if (state == SimulationState::Running) {
            visualTime += deltaRealTime * speedMultiplier;
            simBudget  += deltaRealTime * speedMultiplier;

            while (engine.hasEvents()) {
                const double nextEventTime = engine.peekNextEventTime();
                if (nextEventTime > engine.now() + simBudget) {
                    break;
                }

                const double before = engine.now();
                engine.processEvent();
                const double advanced = engine.now() - before;
                simBudget = std::max(0.0, simBudget - advanced);
            }
        }

        if (!engine.hasEvents()) {
            state = SimulationState::Paused;
        }

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        BeginDockSpaceHost(dock_initialized);

        if (firstFrame && topo.size() == 0) {
            ImGuiFileDialog::Instance()->OpenDialog(
                "TopologyKey",
                "Select Initial Topology",
                ".json"
            );
            firstFrame = false;
        }

        bool stepRequested = false;
        bool engineHasEventsNow = engine.hasEvents();

        renderStatsWindow(
            engine,
            state,
            engine.getStats(),
            buffer,
            packetSize,
            lossProb,
            speedMultiplier,
            stepRequested,
            engineHasEventsNow
        );

        if (stepRequested && state == SimulationState::Paused && engine.hasEvents()) {
            engine.processEvent();
            simBudget = 0.0;
            visualTime += 0.05;
        }

        engineHasEventsNow = engine.hasEvents();

        if (!engineHasEventsNow) {
            state = SimulationState::Paused;
        }

        renderConfigWindow();

        int clicked_node = renderNetworkPanel(
            topo,
            selected_node,
            visualTime,
            activePackets
        );

        if (clicked_node != -1) {
            selected_node = clicked_node;
            routingTable = routing.buildRoutingTable(topo, selected_node);
        }

        renderSelectedNodePanel(selected_node, routingTable);

        if (ImGuiFileDialog::Instance()->Display(
                "TopologyKey",
                ImGuiWindowFlags_NoCollapse,
                ImVec2(400, 300)
            )) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string completePath = ImGuiFileDialog::Instance()->GetFilePathName();
                try {
                    topo = TopologyLoader::load_topology(completePath);

                    engine = SimulationEngine(topo);
                    activePackets.clear();
                    visualTime = 0.0;

                    registerPacketObserver(engine, activePackets, visualTime);

                    engine.setGlobalPacketSize(packetSize);
                    engine.setGlobalLossProb(lossProb);
                    engine.setLatencyObserver([&buffer](double lat) {
                        buffer.addLatencyToBuffer(static_cast<float>(lat));
                    });

                    scheduleDemoTraffic(engine, topo);

                    simBudget    = 0.0;
                    lastRealTime = glfwGetTime();

                    state = SimulationState::Running;
                    selected_node = -1;
                    routingTable.clear();
                } catch (const std::exception& e) {
                    std::cerr << "Load error: " << e.what() << std::endl;
                }
            }

            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    if (argc < 2) {
        return -1;
    }

    Topology topo;

    try {
        topo = TopologyLoader::load_topology(argv[1]);
    } catch (const std::exception&) {
        return -1;
    }

    SimulationState state = SimulationState::Paused;

    SimulationEngine engine(topo);
    CircularBuffer   buffer;

    engine.setLatencyObserver([&buffer](double lat) {
        buffer.addLatencyToBuffer(static_cast<float>(lat));
    });

    int packetSize = 1000;
    engine.setGlobalPacketSize(packetSize);
    engine.setGlobalLossProb(0.0f);

    scheduleDemoTraffic(engine, topo);

    Window      windowMethods;
    GLFWwindow* window = windowMethods.generate_window();

    if (!window) {
        return -1;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    visualizeWindow(engine, topo, state, window, buffer, packetSize);

    shutdownWindow(window);
    return 0;
}