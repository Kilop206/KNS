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

namespace {
    kns::Topology topology() {
        kns::Topology result(2);
        result.addLink(0, 1, 100, 1);
        return result;
    }

    struct TrafficUI {
        gui::TrafficLabPanel panel;
        gui::TranslationService translations;
        kns::SimulationEngine engine{topology()};
        std::string clipboard;
        bool scheduled = false;
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
            ImGui::SetNextWindowSize({1000, 900});
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
