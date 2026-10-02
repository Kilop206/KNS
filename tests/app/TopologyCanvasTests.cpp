#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/include/TopologyCanvas.hpp"
#include "gui/include/PacketRenderer.hpp"
#include "gui/include/TranslationService.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/TopologyLoader.hpp"
#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace {
struct CanvasHarness {
    gui::TopologyCanvas canvas;
    gui::TranslationService translations;
    kns::SimulationEngine engine{kns::Topology{}};
    int selected=-1;
    GLFWwindow* window=nullptr;
    std::optional<std::pair<int,int>> connection;
    std::vector<gui::VisualPacket> packets;
    double visualTime=0;
    CanvasHarness() {
        ImGui::CreateContext();
        auto& io=ImGui::GetIO();
        io.IniFilename=nullptr;
        io.DisplaySize={1280,800};
        io.DeltaTime=1.0f/60;
        io.Fonts->AddFontDefault();
        ImGui::StyleColorsDark();
        if (std::getenv("KNS_CANVAS_CAPTURE")) {
            REQUIRE(glfwInit()==GLFW_TRUE);
            glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
            window=glfwCreateWindow(1280,800,"KNS canvas render test",nullptr,nullptr);
            REQUIRE(window!=nullptr);
            glfwMakeContextCurrent(window);
            REQUIRE(ImGui_ImplOpenGL3_Init("#version 130"));
        } else {
            unsigned char* pixels; int width,height;
            io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        }
        frame(); frame();
    }
    ~CanvasHarness() {
        if (window) { ImGui_ImplOpenGL3_Shutdown(); glfwDestroyWindow(window); glfwTerminate(); }
        ImGui::DestroyContext();
    }
    void frame() {
        if (window) ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({1280,800});
        if (auto request=canvas.render(engine,selected,packets,visualTime,translations)) connection=request;
        ImGui::Render();
        if (window) {
            glViewport(0,0,1280,800); glClearColor(0.05f,0.07f,0.1f,1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glFinish();
        }
    }
    ImGuiWindow* child(const char* name) {
        auto* root=ImGui::FindWindowByName("Network###network-window");
        REQUIRE(root!=nullptr);
        for (auto* child:root->DC.ChildWindows) if (std::strstr(child->Name,name)) return child;
        FAIL("Canvas child was not rendered"); return nullptr;
    }
    ImVec2 paletteTile(int index) {
        const auto p=child("device-palette")->DC.CursorStartPos;
        const float header=2*(ImGui::GetFontSize()+ImGui::GetStyle().ItemSpacing.y)+1+2*ImGui::GetStyle().ItemSpacing.y;
        return {p.x+34+(index%2)*(68+ImGui::GetStyle().ItemSpacing.x),p.y+header+30+(index/2)*(76+ImGui::GetStyle().ItemSpacing.y)};
    }
    void move(ImVec2 p) { ImGui::GetIO().AddMousePosEvent(p.x,p.y); frame(); }
    void click(ImVec2 p) {
        move(p); ImGui::GetIO().AddMouseButtonEvent(0,true); frame();
        ImGui::GetIO().AddMouseButtonEvent(0,false); frame(); frame();
    }
    void rightClick(ImVec2 p) {
        move(p); ImGui::GetIO().AddMouseButtonEvent(1,true); frame();
        ImGui::GetIO().AddMouseButtonEvent(1,false); frame(); frame();
    }
    ImGuiWindow* popup() {
        auto& stack=ImGui::GetCurrentContext()->OpenPopupStack;
        REQUIRE_FALSE(stack.empty());
        REQUIRE(stack.back().Window!=nullptr);
        return stack.back().Window;
    }
    void activate(const char* label, ImGuiWindow* window=nullptr) {
        if (!window) window=ImGui::FindWindowByName("Network###network-window");
        REQUIRE(window!=nullptr);
        ImGui::FocusWindow(window);
        ImGui::ActivateItemByID(window->GetID(label)); frame(); frame();
    }
    void input(const char* label, const char* value) {
        auto* window=popup();
        activate(label,window);
        REQUIRE(ImGui::GetActiveID()==window->GetID(label));
        ImGui::GetIO().AddInputCharactersUTF8(value); frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter,true); frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter,false); frame();
    }
    void drag(ImVec2 from,ImVec2 to) {
        move(from); ImGui::GetIO().AddMouseButtonEvent(0,true); frame();
        move({from.x+12,from.y+12}); move(to); frame();
        ImGui::GetIO().AddMouseButtonEvent(0,false); frame(); frame();
    }
    void capture(const char* name) {
        const auto* directory=std::getenv("KNS_CANVAS_CAPTURE");
        if (!directory) return;
        std::filesystem::create_directories(directory);
        std::vector<unsigned char> pixels(1280*800*3);
        glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,1280,800,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        std::ofstream file(std::filesystem::path(directory)/(std::string(name)+".ppm"),std::ios::binary);
        file<<"P6\n1280 800\n255\n";
        for (int y=799;y>=0;--y) file.write(reinterpret_cast<const char*>(pixels.data()+y*1280*3),1280*3);
    }
};
}

TEST_CASE("Canvas zoom keeps the pointed world location fixed", "[canvas-ui]") {
    gui::CanvasView view;
    const ImVec2 origin{120,80},anchor{630,410};
    const auto original=view.toWorld(anchor,origin);
    for (float zoom : {0.01f,0.5f,1.8f,20.0f}) {
        view.zoomAt(zoom,anchor,origin);
        const auto after=view.toWorld(anchor,origin);
        REQUIRE(after.x==Catch::Approx(original.x)); REQUIRE(after.y==Catch::Approx(original.y));
        const auto screen=view.toScreen(after,origin);
        REQUIRE(screen.x==Catch::Approx(anchor.x)); REQUIRE(screen.y==Catch::Approx(anchor.y));
        REQUIRE(view.zoom>=0.1f); REQUIRE(view.zoom<=2.5f);
    }
}

TEST_CASE("Canvas palette drops create devices and dragging only changes positions", "[canvas-ui]") {
    CanvasHarness ui;
    ui.capture("canvas-empty");
    const auto origin=ui.child("canvas-region")->DC.CursorStartPos;
    const ImVec2 first{origin.x+240,origin.y+180}, second{origin.x+470,origin.y+300};
    ui.drag(ui.paletteTile(0),first);
    REQUIRE(ui.engine.getTopology().size()==1);
    REQUIRE(ui.engine.getTopology().getNode(0)->getDeviceInfo().type==kns::DeviceType::Computer);
    ui.drag(ui.paletteTile(1),second);
    REQUIRE(ui.engine.getTopology().size()==2);
    REQUIRE(ui.engine.getTopology().getNode(1)->getDeviceInfo().type==kns::DeviceType::Router);
    const auto before=*ui.engine.getTopology().getNode(0)->getPosition();
    const auto revision=ui.engine.getTopology().getRoutingRevision();
    // Empty canvas starts at zoom 1, pan (60,60); placement snaps to 24 units.
    const ImVec2 start{origin.x+60+static_cast<float>(before.x),origin.y+60+static_cast<float>(before.y)};
    ui.drag(start,{start.x+96,start.y+72});
    const auto after=*ui.engine.getTopology().getNode(0)->getPosition();
    REQUIRE(after.x==Catch::Approx(before.x+96)); REQUIRE(after.y==Catch::Approx(before.y+72));
    REQUIRE(ui.engine.getTopology().getRoutingRevision()==revision);
    REQUIRE(ui.engine.getTCPSessions().empty()); REQUIRE(ui.engine.getTopology().getLinks().empty());
    REQUIRE_FALSE(ui.connection);
    ui.capture("canvas-moved");
}

TEST_CASE("Canvas cable tool connects devices without starting TCP", "[canvas-ui]") {
    CanvasHarness ui;
    const auto origin=ui.child("canvas-region")->DC.CursorStartPos;
    ui.click(ui.paletteTile(0)); ui.click({origin.x+180,origin.y+180});
    ui.click(ui.paletteTile(1)); ui.click({origin.x+540,origin.y+320});
    REQUIRE(ui.engine.getTopology().size()==2);
    auto* root=ImGui::FindWindowByName("Network###network-window");
    const auto start=root->DC.CursorStartPos;
    const float firstWidth=ImGui::CalcTextSize("Select / Move").x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x;
    ui.click({start.x+firstWidth+12,start.y+10});
    auto point=[&](int id) {
        const auto p=*ui.engine.getTopology().getNode(id)->getPosition();
        return ImVec2{origin.x+60+static_cast<float>(p.x),origin.y+60+static_cast<float>(p.y)};
    };
    ui.click(point(0)); ui.click(point(1));
    REQUIRE(ui.engine.getTopology().getLinks().size()==1);
    REQUIRE(ui.engine.getTCPSessions().empty());
    ui.click(point(0)); ui.click(point(1));
    REQUIRE(ui.engine.getTopology().getLinks().size()==1);
    // Switching explicitly to TCP emits a connection request, leaving the cable intact.
    const float cableWidth=ImGui::CalcTextSize("Cable").x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x;
    ui.click({start.x+firstWidth+cableWidth+12,start.y+10});
    ui.click(point(0)); ui.click(point(1));
    REQUIRE(ui.connection==std::optional<std::pair<int,int>>{{0,1}});
    ui.capture("canvas-connected");
}

TEST_CASE("Canvas renders all device silhouettes with a saved layout", "[canvas-ui]") {
    CanvasHarness ui;
    for (int i=0;i<10;++i) {
        const int id=ui.engine.createNode();
        kns::DeviceInfo info; info.type=static_cast<kns::DeviceType>(i);
        ui.engine.getTopology().setNodeDeviceInfo(id,info);
        ui.engine.getTopology().setNodePosition(id,{double(i%5)*185,double(i/5)*190});
        if (i>0) ui.engine.createLink(i-1,i,100,1);
    }
    ui.canvas.reset(); ui.frame(); ui.frame();
    ui.capture("canvas-devices");
    REQUIRE(ImGui::GetDrawData()->TotalVtxCount>1000);
    REQUIRE(ui.engine.getTopology().size()==10);
}

TEST_CASE("Canvas keeps in-flight packet positions after device removal", "[canvas-ui]") {
    CanvasHarness ui;
    const auto origin=ui.child("canvas-region")->DC.CursorStartPos;
    ui.click(ui.paletteTile(0)); ui.click({origin.x+240,origin.y+180});
    ui.click(ui.paletteTile(1)); ui.click({origin.x+540,origin.y+320});
    ui.engine.createLink(0,1,100,1);
    gui::VisualPacket packet;
    packet.from=0; packet.to=1;
    packet.sim_arrival_time=1; packet.visual_duration=1;
    ui.packets.push_back(packet);
    ui.visualTime=0.5;
    ui.frame();

    auto packetVertices=[&]() {
        std::vector<ImVec2> vertices;
        const auto color=gui::PacketRenderer{}.packetColorByType(kns::PacketType::DATA);
        for (const auto* list:ImGui::GetDrawData()->CmdLists)
            for (const auto& vertex:list->VtxBuffer)
                if (vertex.col==color) vertices.push_back(vertex.pos);
        return vertices;
    };
    const auto before=packetVertices();
    REQUIRE(before.size()>3);
    int removed=0;
    SECTION("source removed") { removed=0; }
    SECTION("destination removed") { removed=1; }
    SECTION("both endpoints removed") { REQUIRE(ui.engine.deleteNode(1)); }
    const auto position=*ui.engine.getTopology().getNode(removed)->getPosition();
    REQUIRE(ui.engine.deleteNode(removed));
    REQUIRE(ui.engine.getTopology().getLinks().empty());
    for (int frame=0;frame<2;++frame) {
        ui.frame();
        const auto after=packetVertices();
        REQUIRE(after.size()==before.size());
        for (std::size_t i=0;i<before.size();++i) {
            REQUIRE(after[i].x==Catch::Approx(before[i].x));
            REQUIRE(after[i].y==Catch::Approx(before[i].y));
        }
    }
    // A removed endpoint anchors existing animations but cannot be selected.
    ui.click({origin.x+60+static_cast<float>(position.x),origin.y+60+static_cast<float>(position.y)});
    REQUIRE(ui.selected==-1);
}

TEST_CASE("Canvas route inspection refreshes without creating traffic", "[canvas-ui][route-trace]") {
    CanvasHarness ui;
    const auto origin=ui.child("canvas-region")->DC.CursorStartPos;
    for (const auto point:{ImVec2{180,180},ImVec2{360,360},ImVec2{650,180}}) {
        ui.click(ui.paletteTile(0)); ui.click({origin.x+point.x,origin.y+point.y});
    }
    auto direct=ui.engine.createLink(0,2,10,1);
    ui.engine.createLink(0,1,100,2);
    auto last=ui.engine.createLink(1,2,100,2);
    const auto revision=ui.engine.getTopology().getRoutingRevision();
    auto toolbar=[&](const char* name) {
        const auto start=ImGui::FindWindowByName("Network###network-window")->DC.CursorStartPos;
        float x=start.x;
        for (const char* label:{"Select / Move","Cable","TCP","Route"}) {
            if (std::strcmp(label,name)==0) { ui.click({x+12,start.y+10}); return; }
            x+=ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().FramePadding.x+ImGui::GetStyle().ItemSpacing.x;
        }
        FAIL("Unknown toolbar button");
    };
    auto nodePoint=[&](int id) {
        const auto p=*ui.engine.getTopology().getNode(id)->getPosition();
        return ImVec2{origin.x+60+static_cast<float>(p.x),origin.y+60+static_cast<float>(p.y)};
    };
    auto routeVertices=[]() {
        std::vector<ImVec2> vertices;
        for (const auto* list:ImGui::GetDrawData()->CmdLists)
            for (const auto& vertex:list->VtxBuffer)
                if (vertex.col==IM_COL32(94,224,174,255)) vertices.push_back(vertex.pos);
        return vertices;
    };
    auto verticalExtent=[&]() {
        const auto vertices=routeVertices();
        REQUIRE_FALSE(vertices.empty());
        float lo=vertices.front().y,hi=lo;
        for (const auto p:vertices) { lo=std::min(lo,p.y); hi=std::max(hi,p.y); }
        return hi-lo;
    };
    toolbar("Route"); ui.click(nodePoint(0)); ui.click(nodePoint(2));
    REQUIRE(verticalExtent()<10);
    REQUIRE_FALSE(ui.connection);
    REQUIRE_FALSE(ui.engine.hasEvents());
    REQUIRE(ui.engine.getTCPSessions().empty());
    REQUIRE(ui.engine.getTopology().getRoutingRevision()==revision);
    REQUIRE(ui.engine.now()==0);
    ui.capture("canvas-route-direct");

    SECTION("live changes select an alternate path and report disconnection") {
        ui.engine.setRoutingMetric(kns::RoutingMetric::Bandwidth); ui.frame();
        REQUIRE(verticalExtent()>100);
        ui.frame(); ui.capture("canvas-route-bandwidth");
        last->setUp(false); ui.frame();
        REQUIRE(verticalExtent()<10);
        direct->setUp(false); ui.frame();
        REQUIRE(routeVertices().empty());
        direct->setUp(true); ui.frame();
        REQUIRE_FALSE(routeVertices().empty());
        REQUIRE(ui.engine.deleteNode(2)); ui.frame();
        REQUIRE(routeVertices().empty());
    }
    SECTION("switching tools clears the route") {
        toolbar("Select / Move");
        REQUIRE(routeVertices().empty());
    }
    SECTION("Escape clears the route") {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true); ui.frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false); ui.frame();
        REQUIRE(routeVertices().empty());
    }
    SECTION("reset clears the route") {
        ui.canvas.reset(); ui.frame();
        REQUIRE(routeVertices().empty());
    }
}

TEST_CASE("Canvas creates configured cables and preserves their properties in JSON", "[canvas-ui][cable-settings]") {
    CanvasHarness ui;
    const auto origin=ui.child("canvas-region")->DC.CursorStartPos;
    const ImVec2 first{origin.x+180,origin.y+180},second{origin.x+540,origin.y+320};
    ui.click(ui.paletteTile(0)); ui.click(first);
    ui.click(ui.paletteTile(1)); ui.click(second);
    ui.activate("Cable settings");
    ui.input("Bandwidth (Mbps)","25");
    ui.input("Delay (ms)","8.5");
    ui.input("Loss (%)","12.5");
    ui.input("Queue capacity","7");
    kns::LinkMode mode=kns::LinkMode::FULL_DUPLEX;
    SECTION("full duplex") { ui.activate("Full duplex",ui.popup()); }
    SECTION("half duplex") { mode=kns::LinkMode::HALF_DUPLEX; ui.activate("Half duplex",ui.popup()); }
    SECTION("simplex") { mode=kns::LinkMode::SIMPLEX; ui.activate("Simplex",ui.popup()); }
    ui.capture("canvas-cable-settings");
    ui.activate("Done",ui.popup());
    ui.activate("Cable"); ui.click(first); ui.click(second);
    REQUIRE(ui.engine.getTopology().getLinks().size()==1);
    const auto link=ui.engine.getTopology().getLinks().front();
    REQUIRE(link->getBandwidthMbps()==25);
    REQUIRE(link->getDelayMs()==8.5);
    REQUIRE(link->getLossProb()==0.125);
    REQUIRE(link->getQueueCapacity()==7);
    REQUIRE(link->getMode()==mode);
    REQUIRE(ui.engine.traceRoute(0,1).status==kns::RouteStatus::Reachable);
    REQUIRE(ui.engine.traceRoute(1,0).status==(mode==kns::LinkMode::SIMPLEX ? kns::RouteStatus::Unreachable : kns::RouteStatus::Reachable));
    const auto restored=kns::TopologyLoader::fromJson(kns::TopologyLoader::toJson(ui.engine.getTopology()));
    const auto saved=restored.getLinks().front();
    REQUIRE(saved->getMode()==mode);
    REQUIRE(saved->getQueueCapacity()==7);
    REQUIRE(saved->getBandwidthMbps()==25);
    REQUIRE(saved->getDelayMs()==8.5);
    REQUIRE(saved->getLossProb()==0.125);
    ui.activate("Cable settings"); ui.activate("Reset defaults",ui.popup()); ui.activate("Done",ui.popup());
    REQUIRE(link->getMode()==mode);
    REQUIRE(link->getQueueCapacity()==7);
    REQUIRE(link->getBandwidthMbps()==25);
    REQUIRE_FALSE(ui.engine.hasEvents());
    REQUIRE(ui.engine.getTCPSessions().empty());
    REQUIRE_FALSE(ui.connection);
    REQUIRE(ui.engine.now()==0);
}

TEST_CASE("Canvas invalid cable settings do not partially create a link", "[canvas-ui][cable-settings]") {
    CanvasHarness ui;
    const auto origin=ui.child("canvas-region")->DC.CursorStartPos;
    const ImVec2 first{origin.x+180,origin.y+180},second{origin.x+540,origin.y+320};
    ui.click(ui.paletteTile(0)); ui.click(first);
    ui.click(ui.paletteTile(1)); ui.click(second);
    const auto revision=ui.engine.getTopology().getRoutingRevision();
    ui.activate("Cable settings");
    SECTION("zero bandwidth") { ui.input("Bandwidth (Mbps)","0"); }
    SECTION("negative delay") { ui.input("Delay (ms)","-1"); }
    SECTION("excessive loss") { ui.input("Loss (%)","101"); }
    SECTION("invalid capacity") { ui.input("Queue capacity","0"); }
    ui.activate("Done",ui.popup());
    ui.activate("Cable"); ui.click(first); ui.click(second);
    REQUIRE(ui.engine.getTopology().getLinks().empty());
    REQUIRE(ui.engine.getTopology().getInterfaces().empty());
    REQUIRE(ui.engine.getTopology().getRoutingRevision()==revision);
    REQUIRE_FALSE(ui.engine.hasEvents());
    ui.activate("Cable settings"); ui.activate("Reset defaults",ui.popup()); ui.activate("Done",ui.popup());
    ui.click(first); ui.click(second);
    REQUIRE(ui.engine.getTopology().getLinks().size()==1);
    const auto link=ui.engine.getTopology().getLinks().front();
    REQUIRE(link->getBandwidthMbps()==100);
    REQUIRE(link->getDelayMs()==1);
    REQUIRE(link->getLossProb()==0);
    REQUIRE(link->getMode()==kns::LinkMode::FULL_DUPLEX);
    REQUIRE(link->getQueueCapacity()==32);
}

TEST_CASE("Canvas cable edits preserve identity and reject unsafe changes", "[canvas-ui][cable-settings]") {
    CanvasHarness ui;
    const auto origin=ui.child("canvas-region")->DC.CursorStartPos;
    ui.click(ui.paletteTile(0)); ui.click({origin.x+180,origin.y+180});
    ui.click(ui.paletteTile(1)); ui.click({origin.x+540,origin.y+320});
    const auto link=ui.engine.createLink(0,1,100,1);
    const auto id=link->getId();
    const auto revision=ui.engine.getTopology().getRoutingRevision();
    const auto a=*ui.engine.getTopology().getNode(0)->getPosition();
    const auto b=*ui.engine.getTopology().getNode(1)->getPosition();
    ui.rightClick({origin.x+60+static_cast<float>((a.x+b.x)/2),origin.y+60+static_cast<float>((a.y+b.y)/2)});
    SECTION("idle mode and capacity changes") {
        ui.input("Queue capacity","5");
        ui.activate("Simplex",ui.popup());
        REQUIRE(link->getQueueCapacity()==5);
        REQUIRE(link->getMode()==kns::LinkMode::SIMPLEX);
        REQUIRE(ui.engine.getTopology().getRoutingRevision()>revision);
        REQUIRE(ui.engine.traceRoute(1,0).status==kns::RouteStatus::Unreachable);
        ui.capture("canvas-cable-edit");
    }
    SECTION("pending transmissions prevent mode changes and shrinking") {
        link->enqueueTransmission(0,1,0,1);
        link->enqueueTransmission(0,1,1,2);
        ui.input("Queue capacity","1");
        REQUIRE(link->getQueueCapacity()==32);
        ui.activate("Half duplex",ui.popup());
        REQUIRE(link->getMode()==kns::LinkMode::FULL_DUPLEX);
        REQUIRE(link->getQueueSize()==2);
        REQUIRE(ui.engine.getTopology().getRoutingRevision()==revision);
        ui.capture("canvas-cable-rejected");
        REQUIRE(link->dequeueTransmission(0,1,0,1));
        REQUIRE(link->dequeueTransmission(0,1,1,2));
        ui.activate("Half duplex",ui.popup());
        REQUIRE(link->getMode()==kns::LinkMode::HALF_DUPLEX);
    }
    REQUIRE(link->getId()==id);
    REQUIRE_FALSE(ui.engine.hasEvents());
    REQUIRE(ui.engine.getTCPSessions().empty());
    REQUIRE_FALSE(ui.connection);
}
