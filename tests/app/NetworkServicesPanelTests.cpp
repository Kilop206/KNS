#include <catch2/catch_test_macros.hpp>
#include "gui/include/NetworkServicesPanel.hpp"
#include "gui/include/TranslationService.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/services/DeviceCLI.hpp"
#include "imgui_internal.h"

namespace {
struct ServicesUI {
    gui::NetworkServicesPanel panel;
    gui::TranslationService translations;
    kns::SimulationEngine engine{kns::Topology(2)};
    int device = 0;
    bool scheduled = false;
    ServicesUI() {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1000, 900};
        io.DeltaTime = 1.0f / 60;
        io.Fonts->AddFontDefault();
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        frame(); frame();
    }
    ~ServicesUI() { ImGui::DestroyContext(); }
    void frame() {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({1000, 900});
        scheduled |= panel.render(engine, device, translations);
        ImGui::Render();
    }
    void activate(const char* label) {
        auto* window = ImGui::FindWindowByName("Device Services###device-services-window");
        REQUIRE(window != nullptr);
        auto* tabs = ImGui::TabBarFindByID(window->GetID("service-tabs"));
        REQUIRE(tabs != nullptr);
        ImGui::FocusWindow(window);
        ImGui::ActivateItemByID(ImHashStr(label, 0, tabs->SelectedTabId));
        frame(); frame();
    }
    void tab(int index) {
        auto* window = ImGui::FindWindowByName("Device Services###device-services-window");
        auto* tabs = ImGui::TabBarFindByID(window->GetID("service-tabs"));
        REQUIRE(tabs != nullptr);
        REQUIRE(tabs->Tabs.Size > index);
        tabs->NextSelectedTabId = tabs->Tabs[index].ID;
        frame(); frame();
    }
};
}

TEST_CASE("Services GUI client schedules requests and embedded CLI shares service configuration", "[services][services-ui]") {
    ServicesUI ui;
    ui.activate("Add service###Add service");
    ui.activate("web (http:80)");
    ui.activate("Save entry###Save entry");
    ui.tab(1);
    ui.activate("Send request###Send request");
    REQUIRE(ui.scheduled);
    REQUIRE(ui.engine.networkServices().requests().size() == 1);
    ui.engine.run();
    ui.frame();
    REQUIRE(ui.engine.networkServices().requests().begin()->second.response == "Hello from KNS");
    ui.tab(2);
    ui.activate("Command###Command");
    ImGui::GetIO().AddInputCharactersUTF8("service add dns names 53");
    ui.frame();
    ui.activate("Execute###Execute");
    REQUIRE(ui.engine.getTopology().getNode(0)->getServices().size() == 2);
    REQUIRE(kns::DeviceCLI::execute(ui.engine, 0, "service record names web.example 192.0.2.1").ok);
    ui.tab(0);
    ui.activate("names (dns:53)");
    ui.frame();
    REQUIRE(ImGui::GetDrawData()->TotalVtxCount > 0);
}

TEST_CASE("Services GUI creates edits stops and removes a service on the selected device", "[services][services-ui]") {
    ServicesUI ui;
    ui.activate("Add service###Add service");
    REQUIRE(ui.engine.getTopology().getNode(0)->getServices().size() == 1);
    REQUIRE(ui.engine.getTopology().getNode(1)->getServices().empty());
    ui.activate("web (http:80)");
    ui.activate("Save entry###Save entry");
    REQUIRE(ui.engine.getTopology().getNode(0)->getServices()[0].pages.at("/").body == "Hello from KNS");
    ui.activate("Running###Running");
    REQUIRE_FALSE(ui.engine.getTopology().getNode(0)->getServices()[0].enabled);
    ui.activate("Remove entry###Remove entry");
    REQUIRE(ui.engine.getTopology().getNode(0)->getServices()[0].pages.empty());
    ui.device = 1;
    ui.frame();
    ui.activate("Add service###Add service");
    REQUIRE(ui.engine.getTopology().getNode(1)->getServices().size() == 1);
    ui.device = 0;
    ui.frame();
    ui.activate("web (http:80)");
    ui.activate("Remove service###Remove service");
    REQUIRE(ui.engine.getTopology().getNode(0)->getServices().empty());
    REQUIRE(ui.engine.getTopology().getNode(1)->getServices().size() == 1);
    ui.engine.deleteNode(1);
    ui.device = 1;
    REQUIRE_NOTHROW(ui.frame());
    ui.panel.reset();
    ui.device = -1;
    REQUIRE_NOTHROW(ui.frame());
}
