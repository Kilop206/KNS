#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/include/TrafficLabPanel.hpp"
#include "gui/include/TranslationService.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "traffic/TrafficRun.hpp"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>

namespace {
    kns::Topology topology() {
        kns::Topology result(2);
        result.addLink(0, 1, 100, 1);
        return result;
    }

    nlohmann::json importedPlan() {
        return {{"schema_version", "1.0"}, {"seed", std::numeric_limits<std::uint64_t>::max()},
            {"max_packets", 100}, {"flows", nlohmann::json::array({
                {{"id", 70007}, {"source", 0}, {"destination", 1}, {"wire_bytes", 1000},
                 {"start_s", 0}, {"end_s", 0.02}, {"priority", 7}, {"ttl_hops", 12},
                 {"deadline_s", 0.003},
                 {"schedule", {{"kind", "custom"}, {"offsets_s", {0, 0.01}}}},
                 {"application", {{"kind", "api"}, {"response_wire_bytes", 200},
                     {"processing_delay_s", 0.002}, {"deadline_s", 0.005}}}}
            })}};
    }

    struct TrafficUI {
        gui::TrafficLabPanel panel;
        gui::TranslationService translations;
        kns::SimulationEngine engine{topology()};
        std::string clipboard;
        bool scheduled = false;
        float panelWidth = 1000;
        GLFWwindow* window = nullptr;

        TrafficUI() {
            kns::RunConfig config;
            config.seed = 99;
            engine.configureRun(config);
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = {1000, 900};
            io.DeltaTime = 1.0f / 60;
            io.Fonts->AddFontDefault();
            auto& platform = ImGui::GetPlatformIO();
            platform.Platform_ClipboardUserData = &clipboard;
            platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
                *static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData) = text;
            };
            platform.Platform_GetClipboardTextFn = [](ImGuiContext*) {
                return static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData)->c_str();
            };
            if (std::getenv("KNS_TRAFFIC_CAPTURE")) {
                REQUIRE(glfwInit() == GLFW_TRUE);
                glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                window = glfwCreateWindow(1000, 900, "Traffic Lab UI test", nullptr, nullptr);
                REQUIRE(window != nullptr);
                glfwMakeContextCurrent(window);
                REQUIRE(ImGui_ImplOpenGL3_Init("#version 130"));
            } else {
                unsigned char* pixels;
                int width, height;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            }
            frame();
            frame();
        }

        ~TrafficUI() {
            if (window) {
                ImGui_ImplOpenGL3_Shutdown();
                glfwDestroyWindow(window);
                glfwTerminate();
            }
            ImGui::DestroyContext();
        }

        void frame(kns::SimulationEngine* replacement = nullptr) {
            if (window)
                ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize({panelWidth, 900});
            scheduled |= panel.render(replacement ? *replacement : engine, translations, 99);
            ImGui::Render();
            if (window) {
                glViewport(0, 0, 1000, 900);
                glClearColor(0.05f, 0.07f, 0.1f, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                glFinish();
            }
        }

        ImGuiWindow* panelWindow() {
            auto* result = ImGui::FindWindowByName("Traffic Lab###traffic-lab-window");
            REQUIRE(result != nullptr);
            return result;
        }

        void activate(const char* id) {
            auto* target = panelWindow();
            ImGui::FocusWindow(target);
            ImGui::ActivateItemByID(target->GetID((std::string("###") + id).c_str()));
            frame();
            frame();
        }

        void input(const char* id, const char* value) {
            activate(id);
            auto* input = ImGui::GetInputTextState(ImGui::GetActiveID());
            REQUIRE(input != nullptr);
            input->SelectAll();
            ImGui::GetIO().AddInputCharactersUTF8(value);
            frame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
            frame();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
            frame();
        }

        void capture(const char* name) {
            const auto* directory = std::getenv("KNS_TRAFFIC_CAPTURE");
            if (!directory)
                return;
            std::filesystem::create_directories(directory);
            std::vector<unsigned char> pixels(1000 * 900 * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, 1000, 900, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            std::ofstream output(std::filesystem::path(directory) / (std::string(name) + ".ppm"),
                                 std::ios::binary);
            output << "P6\n1000 900\n255\n";
            for (int y = 899; y >= 0; --y)
                output.write(reinterpret_cast<const char*>(pixels.data() + y * 1000 * 3), 3000);
        }
    };
}

TEST_CASE("Traffic Lab drafts produce native plans for all four GUI patterns", "[traffic-lab-ui]") {
    for (auto pattern : {gui::TrafficPattern::Periodic, gui::TrafficPattern::CBR,
                         gui::TrafficPattern::Burst, gui::TrafficPattern::Poisson}) {
        gui::TrafficFlowDraft draft;
        draft.pattern = pattern;
        draft.deadline = true;
        draft.deadlineMs = 2.5;
        const auto flow = draft.toJson(7);
        REQUIRE(flow["deadline_s"] == 0.0025);
        REQUIRE(flow["end_s"] == 1);
        nlohmann::json plan{{"schema_version", "1.0"}, {"seed", 42}, {"max_packets", 10000},
                            {"flows", nlohmann::json::array({flow})}};
        kns::SimulationEngine engine(topology());
        REQUIRE_NOTHROW(kns::traffic::TrafficRun::validatePlan(engine, plan));
        REQUIRE_FALSE(engine.hasEvents());
        auto run = kns::traffic::TrafficRun::schedule(engine, plan);
        engine.run();
        const auto report = run->toJson(engine.now());
        REQUIRE(report["generated"].get<int>() > 0);
        REQUIRE(report["pending"] == 0);
        REQUIRE(report["delivered"] == report["generated"]);
        if (pattern == gui::TrafficPattern::Periodic) {
            REQUIRE(flow["schedule"]["interval_s"] == 0.1);
            REQUIRE(report["generated"] == 10);
        } else if (pattern == gui::TrafficPattern::CBR) {
            REQUIRE(flow["schedule"]["rate_bps"] == 1000000);
            REQUIRE(report["generated"] == 125);
        } else if (pattern == gui::TrafficPattern::Burst) {
            REQUIRE(flow["schedule"]["burst_interval_s"] == 0.25);
            REQUIRE(flow["schedule"]["packet_interval_s"] == 0.001);
            REQUIRE(report["generated"] == 40);
        } else {
            REQUIRE(flow["schedule"]["rate_pps"] == 100);
        }
    }
}

TEST_CASE("Traffic Lab schedules concurrent GUI flows with actual deadline and delivery outcomes",
          "[traffic-lab-ui]") {
    TrafficUI ui;
    ui.activate("traffic-deadline-enabled");
    ui.input("traffic-deadline", "0.5");
    ui.activate("traffic-add");
    ui.input("traffic-source", "1");
    ui.input("traffic-destination", "0");
    ui.activate("traffic-deadline-enabled");
    ui.capture("traffic-editor");
    ui.activate("traffic-copy-plan");
    const auto plan = nlohmann::json::parse(ui.clipboard);
    REQUIRE(plan["flows"].size() == 2);
    REQUIRE(plan["flows"][0]["source"] == 0);
    REQUIRE(plan["flows"][1]["source"] == 1);
    REQUIRE_FALSE(ui.engine.hasEvents());
    ui.activate("traffic-schedule");
    REQUIRE(ui.scheduled);
    REQUIRE(ui.engine.pendingEventCount() == 2);
    REQUIRE(ui.engine.now() == 0);
    ui.engine.run();
    ui.frame();
    const auto report = *ui.panel.report(ui.engine);
    REQUIRE(report["generated"] == 20);
    REQUIRE(report["delivered"] == 20);
    REQUIRE(report["pending"] == 0);
    REQUIRE(report["flows"][0]["delivered_late"] == 10);
    REQUIRE(report["flows"][1]["delivered_late"] == 0);
    REQUIRE(report["network_seed"] == 99);
    REQUIRE_FALSE(report["generation_pending"].get<bool>());
    REQUIRE_FALSE(report["execution_interrupted"].get<bool>());
    ui.capture("traffic-results");
    ui.activate("traffic-copy-report");
    REQUIRE(nlohmann::json::parse(ui.clipboard) == report);
    ui.activate("traffic-configuration");
    ui.activate("traffic-flow");
    const auto& popups = ImGui::GetCurrentContext()->OpenPopupStack;
    REQUIRE_FALSE(popups.empty());
    auto* popup = popups.back().Window;
    REQUIRE(popup != nullptr);
    ImGui::FocusWindow(popup);
    ImGui::ActivateItemByID(popup->GetID("1"));
    ui.frame();
    ui.frame();
    ui.engine.getTopology().removeNode(0);
    ui.activate("traffic-copy-plan");
    REQUIRE(nlohmann::json::parse(ui.clipboard) == plan);
}

TEST_CASE("Traffic Lab rejects invalid drafts without partial scheduling and can retry",
          "[traffic-lab-ui]") {
    TrafficUI ui;
    ui.input("traffic-interval", "0");
    ui.activate("traffic-schedule");
    REQUIRE_FALSE(ui.scheduled);
    REQUIRE_FALSE(ui.engine.hasEvents());
    REQUIRE_FALSE(ui.panel.report(ui.engine));
    ui.input("traffic-interval", "100");
    ui.activate("traffic-schedule");
    REQUIRE(ui.scheduled);
    REQUIRE(ui.engine.pendingEventCount() == 1);
    ui.activate("traffic-schedule");
    REQUIRE(ui.engine.pendingEventCount() == 1);
}

TEST_CASE("Traffic Lab does not mix a workload into an existing TCP run", "[traffic-lab-ui]") {
    TrafficUI ui;
    ui.engine.startTCPConnection(0, 1);
    const auto pending = ui.engine.pendingEventCount();
    ui.activate("traffic-schedule");
    REQUIRE_FALSE(ui.scheduled);
    REQUIRE(ui.engine.pendingEventCount() == pending);
    REQUIRE_FALSE(ui.panel.report(ui.engine));
}

TEST_CASE("A sparse Poisson workload can be scheduled and drain without any source messages",
          "[traffic-lab-ui]") {
    TrafficUI ui;
    ui.activate("traffic-pattern");
    const auto& stack = ImGui::GetCurrentContext()->OpenPopupStack;
    REQUIRE_FALSE(stack.empty());
    auto* popup = stack.back().Window;
    REQUIRE(popup != nullptr);
    const int index = 3;
    const auto seed = ImHashData(&index, sizeof(index), popup->IDStack.back());
    ImGui::FocusWindow(popup);
    ImGui::ActivateItemByID(ImHashStr("Poisson", 0, seed));
    ui.frame();
    ui.frame();
    ui.input("traffic-pps", "0.001");
    ui.activate("traffic-schedule");
    REQUIRE(ui.scheduled);
    REQUIRE_FALSE(ui.engine.hasEvents());
    const auto report = *ui.panel.report(ui.engine);
    REQUIRE(report["generated"] == 0);
    REQUIRE(report["pending"] == 0);
    REQUIRE_FALSE(report["generation_pending"].get<bool>());
    REQUIRE_FALSE(report["generation_limit_reached"].get<bool>());
}

TEST_CASE("Traffic Lab distinguishes truncation and interruption and clears replaced-engine results",
          "[traffic-lab-ui]") {
    TrafficUI ui;
    ui.input("traffic-budget", "2");
    ui.activate("traffic-schedule");
    REQUIRE(ui.scheduled);
    SECTION("packet budget is a partial workload even after draining") {
        ui.engine.run();
        const auto report = *ui.panel.report(ui.engine);
        REQUIRE(report["generation_limit_reached"].get<bool>());
        REQUIRE(report["generated"] == 2);
        REQUIRE(report["pending"] == 0);
    }
    SECTION("cancellation leaves explicit interruption evidence") {
        ui.engine.cancel();
        const auto report = *ui.panel.report(ui.engine);
        REQUIRE(report["execution_interrupted"].get<bool>());
        REQUIRE(report["generation_pending"].get<bool>());
    }
    kns::SimulationEngine replacement(topology());
    REQUIRE_FALSE(ui.panel.report(replacement));
    ui.frame(&replacement);
    REQUIRE_FALSE(ui.panel.report(replacement));
    REQUIRE_FALSE(ui.panel.report(ui.engine));
}

TEST_CASE("Traffic Lab imports advanced plans losslessly and exposes real operation outcomes",
          "[traffic-lab-ui][traffic-import]") {
    TrafficUI ui;
    const auto plan = importedPlan();
    const auto revision = ui.engine.getTopology().getRoutingRevision();
    ui.clipboard = plan.dump();
    ui.activate("traffic-paste-plan");
    REQUIRE_FALSE(ui.engine.hasEvents());
    REQUIRE(ui.engine.getTopology().getRoutingRevision() == revision);
    ui.activate("traffic-imported-flows");
    ui.capture("traffic-imported-plan");
    ui.activate("traffic-copy-plan");
    REQUIRE(nlohmann::json::parse(ui.clipboard) == plan);
    ui.activate("traffic-schedule");
    REQUIRE(ui.scheduled);
    ui.engine.run();
    ui.frame();
    const auto report = *ui.panel.report(ui.engine);
    REQUIRE(report["config"] == plan);
    REQUIRE(report["generated"] == 4);
    REQUIRE(report["delivered"] == 4);
    const auto& flow = report["flows"][0];
    REQUIRE(flow["id"] == 70007);
    REQUIRE(flow["operations_started"] == 2);
    REQUIRE(flow["operations_completed"] == 2);
    REQUIRE(flow["operations_on_time"] == 2);
    REQUIRE(flow["operations_pending"] == 0);
    REQUIRE(flow["operation_latency"]["p95_s"].get<double>() == Catch::Approx(0.004096));
    REQUIRE(ImGui::TableFindByID(ui.panelWindow()->GetID("traffic-operations")) != nullptr);
    ui.capture("traffic-imported-operations");
    ui.activate("traffic-copy-report");
    REQUIRE(nlohmann::json::parse(ui.clipboard) == report);
}

TEST_CASE("Rejected clipboard imports preserve the previous plan and the editor drafts",
          "[traffic-lab-ui][traffic-import]") {
    TrafficUI ui;
    ui.input("traffic-budget", "123");
    ui.activate("traffic-copy-plan");
    const auto draft = nlohmann::json::parse(ui.clipboard);
    const auto plan = importedPlan();
    ui.clipboard = plan.dump();
    ui.activate("traffic-paste-plan");
    auto invalid = plan;
    invalid["flows"][0]["source"] = 99;
    for (const auto& text : {std::string("{"), "{\"seed\":1," + plan.dump().substr(1),
                             invalid.dump(), std::string(4 * 1024 * 1024 + 1, ' ')}) {
        ui.clipboard = text;
        ui.activate("traffic-paste-plan");
        REQUIRE_FALSE(ui.engine.hasEvents());
        ui.activate("traffic-copy-plan");
        REQUIRE(nlohmann::json::parse(ui.clipboard) == plan);
    }
    ui.activate("traffic-use-drafts");
    ui.activate("traffic-copy-plan");
    REQUIRE(nlohmann::json::parse(ui.clipboard) == draft);
    REQUIRE_FALSE(ui.engine.hasEvents());
}

TEST_CASE("Imported plans retain native flow limits beyond the simplified editor",
          "[traffic-lab-ui][traffic-import]") {
    TrafficUI ui;
    auto plan = importedPlan();
    for (int i = 1; i < 17; ++i) {
        auto flow = plan["flows"][0];
        flow["id"] = 70007 + i;
        plan["flows"].push_back(flow);
    }
    ui.clipboard = plan.dump();
    ui.activate("traffic-paste-plan");
    ui.activate("traffic-schedule");
    REQUIRE(ui.scheduled);
    REQUIRE(ui.engine.pendingEventCount() == 17);
    ui.engine.run();
    ui.frame();
    const auto report = *ui.panel.report(ui.engine);
    REQUIRE(report["flows"].size() == 17);
    REQUIRE(report["config"] == plan);
    REQUIRE(report["generated"] == 68);
    REQUIRE(report["delivered"] == 68);
    auto* table = ImGui::TableFindByID(ui.panelWindow()->GetID("traffic-outcomes"));
    REQUIRE(table != nullptr);
    REQUIRE(table->InnerRect.GetHeight() <= 200);
    REQUIRE(table->InnerRect.GetHeight() > 100);
}

TEST_CASE("Clipboard replay plans preserve noncontiguous IDs and tied source order",
          "[traffic-lab-ui][traffic-import]") {
    TrafficUI ui;
    auto plan = importedPlan();
    auto& first = plan["flows"][0];
    first["id"] = 12;
    first.erase("application");
    first["schedule"] = {{"kind", "replay"}};
    auto second = first;
    second["id"] = 900009;
    plan["flows"].push_back(second);
    plan["replay_events"] = {{{"at_s", 0}, {"flow_id", 900009}},
                              {{"at_s", 0}, {"flow_id", 12}}};
    std::vector<std::uint32_t> sourceOrder;
    ui.engine.setPacketObserver([&](const kns::Packet& packet, auto, auto, auto, auto, auto) {
        if (packet.workload && packet.hop_count == 0)
            sourceOrder.push_back(packet.workload->flow_id);
    });
    ui.clipboard = plan.dump();
    ui.activate("traffic-paste-plan");
    ui.activate("traffic-schedule");
    REQUIRE(ui.scheduled);
    REQUIRE(ui.engine.pendingEventCount() == 1);
    ui.engine.run();
    REQUIRE(sourceOrder == std::vector<std::uint32_t>{900009, 12});
    const auto report = *ui.panel.report(ui.engine);
    REQUIRE(report["config"] == plan);
    REQUIRE(report["generated"] == 2);
    REQUIRE(report["delivered"] == 2);
}

TEST_CASE("Imported plan controls remain usable in a narrow dock", "[traffic-lab-ui][traffic-import]") {
    TrafficUI ui;
    ui.panelWidth = 300;
    ui.frame();
    const auto plan = importedPlan();
    ui.clipboard = plan.dump();
    ui.activate("traffic-paste-plan");
    ui.activate("traffic-copy-plan");
    REQUIRE(nlohmann::json::parse(ui.clipboard) == plan);
    ui.capture("traffic-imported-narrow");
    ui.activate("traffic-use-drafts");
    ui.activate("traffic-copy-plan");
    const auto draft = nlohmann::json::parse(ui.clipboard);
    REQUIRE(draft["seed"] == 42);
    REQUIRE(draft["flows"][0]["id"] == 1);
    REQUIRE_FALSE(ui.engine.hasEvents());
}
