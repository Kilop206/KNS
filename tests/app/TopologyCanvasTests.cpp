#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/include/TopologyCanvas.hpp"
#include "gui/include/PacketRenderer.hpp"
#include "gui/include/TranslationService.hpp"
#include "engine/core/SimulationEngine.hpp"
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
