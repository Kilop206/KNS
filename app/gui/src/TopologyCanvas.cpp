#include "TopologyCanvas.hpp"
#include "DeviceIcons.hpp"
#include "PacketRenderer.hpp"
#include "TranslationService.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>

namespace gui {
namespace {
constexpr const char* payloadType = "KNS_DEVICE_TYPE";
constexpr float grid = 24.0f;
ImVec2 nodePoint(const kns::Node& node) {
    const auto& p = *node.getPosition();
    return {static_cast<float>(p.x), static_cast<float>(p.y)};
}
ImVec2 bounded(ImVec2 value, bool snap) {
    if (snap) { value.x = std::round(value.x/grid)*grid; value.y = std::round(value.y/grid)*grid; }
    return {std::clamp(value.x,-1000000.0f,1000000.0f), std::clamp(value.y,-1000000.0f,1000000.0f)};
}
float distanceToLink(ImVec2 point, ImVec2 a, ImVec2 b) {
    const float dx=b.x-a.x, dy=b.y-a.y, length=dx*dx+dy*dy;
    const float t=length > 0 ? std::clamp(((point.x-a.x)*dx+(point.y-a.y)*dy)/length,0.0f,1.0f) : 0;
    return std::hypot(point.x-a.x-dx*t,point.y-a.y-dy*t);
}

bool modePicker(kns::LinkMode& mode, TranslationService& tr) {
    bool changed=false;
    for (const auto& [value,label]:{
            std::pair{kns::LinkMode::FULL_DUPLEX,"Full duplex"},
            std::pair{kns::LinkMode::HALF_DUPLEX,"Half duplex"},
            std::pair{kns::LinkMode::SIMPLEX,"Simplex"}}) {
        if (ImGui::RadioButton(tr.translate(label).c_str(),mode==value)) { mode=value; changed=true; }
    }
    const char* description=mode==kns::LinkMode::FULL_DUPLEX ? "Both directions simultaneously; queue capacity per direction." :
        mode==kns::LinkMode::HALF_DUPLEX ? "Both directions share one transmission queue." :
        "One direction only: first device to second device.";
    ImGui::TextWrapped("%s",tr.translate(description).c_str());
    return changed;
}
}

void TopologyCanvas::reset() { *this = TopologyCanvas{}; }

void TopologyCanvas::cableSettings(TranslationService& tr) {
    if (ImGui::SmallButton(tr.translate("Cable settings").c_str())) ImGui::OpenPopup("cable-settings");
    ImGui::SetNextWindowSize({360,0},ImGuiCond_Always);
    if (ImGui::BeginPopup("cable-settings")) {
        ImGui::TextUnformatted(tr.translate("New cables").c_str());
        ImGui::TextWrapped("%s",tr.translate("Applies to the next cables you create with the Cable tool.").c_str());
        ImGui::Separator();
        ImGui::SetNextItemWidth(140);
        ImGui::InputDouble(tr.translate("Bandwidth (Mbps)").c_str(),&cableOptions_.bandwidthMbps,0,0,"%.2f");
        ImGui::SetNextItemWidth(140);
        ImGui::InputDouble(tr.translate("Delay (ms)").c_str(),&cableOptions_.delayMs,0,0,"%.2f");
        ImGui::SetNextItemWidth(140);
        ImGui::InputDouble(tr.translate("Loss (%)").c_str(),&cableOptions_.lossPercent,0,0,"%.2f");
        ImGui::SetNextItemWidth(140);
        ImGui::InputInt(tr.translate("Queue capacity").c_str(),&cableOptions_.queueCapacity,0,0);
        modePicker(cableOptions_.mode,tr);
        ImGui::Separator();
        if (ImGui::Button(tr.translate("Reset defaults").c_str())) cableOptions_=CableOptions{};
        ImGui::SameLine();
        if (ImGui::Button(tr.translate("Done").c_str())) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void TopologyCanvas::updateRoute(const kns::SimulationEngine& engine) {
    if (!routeEndpoints_) return;
    const auto& topology=engine.getTopology();
    for (int id:{routeEndpoints_->first,routeEndpoints_->second}) {
        const auto* node=topology.getNode(id);
        if (!node || !node->isActive()) { routeEndpoints_.reset(); return; }
    }
    if (routeRevision_==topology.getRoutingRevision() && routeMetric_==engine.getRoutingMetric()) return;
    route_=engine.traceRoute(routeEndpoints_->first,routeEndpoints_->second);
    routeRevision_=topology.getRoutingRevision();
    routeMetric_=engine.getRoutingMetric();
}

void TopologyCanvas::ensurePositions(kns::Topology& topology) {
    int slot = 0;
    const int columns = std::max(3, static_cast<int>(std::ceil(std::sqrt(topology.size()))));
    for (int id=0; id<topology.size(); ++id) {
        const auto* node=topology.getNode(id);
        if (!node->isActive() || node->getPosition()) continue;
        ImVec2 position;
        bool occupied;
        do {
            position = {static_cast<float>(slot%columns)*192, static_cast<float>(slot/columns)*144};
            ++slot;
            occupied = false;
            for (int other=0; other<topology.size(); ++other) {
                const auto* existing=topology.getNode(other);
                if (!existing->isActive() || !existing->getPosition()) continue;
                const auto p=nodePoint(*existing);
                if (std::abs(p.x-position.x)<150 && std::abs(p.y-position.y)<110) { occupied=true; break; }
            }
        } while (occupied);
        topology.setNodePosition(id, {position.x,position.y});
    }
}

void TopologyCanvas::arrange(kns::Topology& topology) {
    std::vector<int> nodes;
    for (int id=0; id<topology.size(); ++id) if (topology.getNode(id)->isActive()) nodes.push_back(id);
    // Put high-degree hubs first in a stable, evenly spaced layout.
    std::stable_sort(nodes.begin(),nodes.end(),[&](int a,int b) {
        return topology.getLinksFromNode(a).size()>topology.getLinksFromNode(b).size();
    });
    const int columns=std::max(1,static_cast<int>(std::ceil(std::sqrt(nodes.size()))));
    int slot=0;
    for (int id:nodes) {
        topology.setNodePosition(id,{static_cast<double>(slot%columns)*192,static_cast<double>(slot/columns)*144});
        ++slot;
    }
    fitPending_=true;
}

void TopologyCanvas::fit(const kns::Topology& topology, ImVec2 size) {
    ImVec2 lo{std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    ImVec2 hi{-lo.x,-lo.y};
    for (int id=0; id<topology.size(); ++id) {
        const auto* node=topology.getNode(id);
        if (!node->isActive() || !node->getPosition()) continue;
        auto p=nodePoint(*node);
        lo.x=std::min(lo.x,p.x-78); lo.y=std::min(lo.y,p.y-45);
        hi.x=std::max(hi.x,p.x+78); hi.y=std::max(hi.y,p.y+68);
    }
    if (lo.x>hi.x) { view_=CanvasView{}; return; }
    view_.zoom=std::clamp(std::min((size.x-50)/std::max(1.0f,hi.x-lo.x),
        (size.y-50)/std::max(1.0f,hi.y-lo.y)),0.1f,1.3f);
    view_.pan={(size.x-(lo.x+hi.x)*view_.zoom)*0.5f,(size.y-(lo.y+hi.y)*view_.zoom)*0.5f};
}

void TopologyCanvas::createDevice(kns::SimulationEngine& engine, kns::DeviceType type,
    ImVec2 position, int& selectedNode) {
    if (engine.getTopology().size()>=4096) throw std::runtime_error("Node limit reached; save and reload to free removed slots.");
    position=bounded(position,snap_);
    const int id=engine.createNode();
    kns::DeviceInfo info;
    info.type=type; info.evidence="user_defined";
    auto& topology=engine.getTopology();
    topology.setNodeDeviceInfo(id,std::move(info));
    topology.setNodeLabel(id,std::string(deviceDisplayName(type))+" "+std::to_string(id));
    topology.setNodePosition(id,{position.x,position.y});
    selectedNode=id; selectedLink_.reset(); error_.clear();
}

void TopologyCanvas::palette(TranslationService& tr) {
    ImGui::TextUnformatted(tr.translate("Devices").c_str());
    ImGui::TextDisabled("%s",tr.translate("Drag onto canvas").c_str());
    ImGui::Separator();
    constexpr kns::DeviceType types[] = {kns::DeviceType::Computer,kns::DeviceType::Router,
        kns::DeviceType::Switch,kns::DeviceType::AccessPoint,kns::DeviceType::Server,
        kns::DeviceType::Phone,kns::DeviceType::Printer,kns::DeviceType::IoT,
        kns::DeviceType::NetworkSegment,kns::DeviceType::Unknown};
    for (int i=0;i<10;++i) {
        const auto type=types[i];
        ImGui::PushID(i);
        if (i%2) ImGui::SameLine();
        const auto start=ImGui::GetCursorScreenPos();
        const bool chosen=placing_ && *placing_==type;
        if (chosen) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.15f,0.40f,0.62f,1));
        if (ImGui::Button("##device",{68,76})) { placing_=type; tool_=Tool::Select; source_=-1; routeEndpoints_.reset(); }
        if (chosen) ImGui::PopStyleColor();
        auto* draw=ImGui::GetWindowDrawList();
        drawDeviceIcon(draw,type,{start.x+34,start.y+27},36);
        const char* name=type==kns::DeviceType::NetworkSegment ? "Segment" :
            (type==kns::DeviceType::AccessPoint ? "Wi-Fi AP" : deviceDisplayName(type));
        const auto label=tr.translate(name);
        const float font=std::min(ImGui::GetFontSize(),11.0f);
        const auto textSize=ImGui::GetFont()->CalcTextSizeA(font,1000,0,label.c_str());
        draw->AddText(ImGui::GetFont(),font,{start.x+34-textSize.x/2,start.y+56},ImGui::GetColorU32(ImGuiCol_Text),label.c_str());
        if (ImGui::BeginDragDropSource()) {
            const int value=static_cast<int>(type);
            ImGui::SetDragDropPayload(payloadType,&value,sizeof(value));
            ImGui::TextUnformatted(tr.translate(deviceDisplayName(type)).c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tr.translate("Drag to create, or click then place on the canvas.").c_str());
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("%s",tr.translate("Move: drag a device\nPan: middle mouse\nZoom: mouse wheel\nCancel: Esc\nRemove: Delete").c_str());
}

std::optional<std::pair<int,int>> TopologyCanvas::render(kns::SimulationEngine& engine, int& selectedNode,
    const std::vector<VisualPacket>& packets, double visualTime, TranslationService& tr) {
    std::optional<std::pair<int,int>> connection;
    const auto title=tr.label("Network","network-window");
    if (!ImGui::Begin(title.c_str(),nullptr,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End(); return connection;
    }
    auto& topology=engine.getTopology();
    auto active=[&](int id) { const auto* n=topology.getNode(id); return n && n->isActive(); };
    if (!active(selectedNode)) selectedNode=-1;
    if (!active(source_)) source_=-1;
    if (!active(dragging_)) dragging_=-1;
    if (selectedLink_ && std::none_of(topology.getLinks().begin(),topology.getLinks().end(),
        [&](const auto& link) { return link->getId()==*selectedLink_; })) selectedLink_.reset();

    auto toolButton=[&](const char* name, Tool tool) {
        const bool current=tool_==tool && !placing_;
        if (current) ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0.16f,0.42f,0.65f,1));
        if (ImGui::Button(tr.translate(name).c_str())) { tool_=tool; placing_.reset(); source_=-1; dragging_=-1; error_.clear(); routeEndpoints_.reset(); }
        if (current) ImGui::PopStyleColor();
    };
    toolButton("Select / Move",Tool::Select); ImGui::SameLine();
    toolButton("Cable",Tool::Cable); ImGui::SameLine();
    toolButton("TCP",Tool::TCP); ImGui::SameLine();
    toolButton("Route",Tool::Route); ImGui::SameLine();
    if (ImGui::Button(tr.translate("Fit").c_str())) fitPending_=true;
    ImGui::SameLine();
    if (ImGui::Button(tr.translate("Arrange").c_str())) { arrange(topology); dragging_=-1; }
    ImGui::Checkbox(tr.translate("Snap to grid").c_str(),&snap_); ImGui::SameLine();
    ImGui::TextDisabled("%.0f%%",view_.zoom*100); ImGui::SameLine();
    ImGui::BeginDisabled(selectedNode<0 && !selectedLink_);
    bool remove=ImGui::SmallButton(tr.translate("Delete selected").c_str());
    ImGui::EndDisabled();
    ImGui::SameLine(); cableSettings(tr);
    updateRoute(engine);
    if (!error_.empty()) ImGui::TextColored(ImVec4(1,0.5f,0.35f,1),"%s",error_.c_str());
    else if (routeEndpoints_) {
        const char* status=route_.status==kns::RouteStatus::Reachable ? "Reachable" :
            route_.status==kns::RouteStatus::ForwardingLoop ? "Forwarding loop" : "Unreachable";
        ImGui::Text("#%d -> #%d: %s | %s (%s)",routeEndpoints_->first,routeEndpoints_->second,
            tr.translate(status).c_str(),tr.translate(kns::routingMetricName(routeMetric_).data()).c_str(),
            tr.translate("hover for details").c_str());
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s: %zu",tr.translate("Hops").c_str(),route_.hops.size());
            ImGui::Text("%s: %.2f ms",tr.translate("Propagation delay").c_str(),route_.propagation_delay_ms);
            if (route_.bottleneck_mbps) ImGui::Text("%s: %.2f Mbps",tr.translate("Bottleneck capacity").c_str(),*route_.bottleneck_mbps);
            ImGui::TextUnformatted(tr.translate("Configured path values; excludes queueing and serialization delay.").c_str());
            if (route_.status!=kns::RouteStatus::Reachable) ImGui::TextUnformatted(tr.translate("Values describe the traversed prefix only.").c_str());
            for (const auto& hop:route_.hops) ImGui::Text("#%d -> #%d | %s %llu",hop.from,hop.to,
                tr.translate("Link").c_str(),static_cast<unsigned long long>(hop.link_id));
            ImGui::EndTooltip();
        }
    }
    else ImGui::TextDisabled("%s",tr.translate(placing_ ? "Click the canvas to place a device. Esc cancels." :
        tool_==Tool::Select ? "Drag to move. Right-click a device or cable to edit." :
        source_<0 ? "Choose the source device." : "Choose the destination device. Esc cancels.").c_str());

    ImGui::BeginChild("device-palette",{154,0},ImGuiChildFlags_Borders);
    palette(tr);
    ImGui::EndChild(); ImGui::SameLine();
    ImGui::BeginChild("canvas-region",{0,0},ImGuiChildFlags_Borders,
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    const auto origin=ImGui::GetCursorScreenPos();
    auto size=ImGui::GetContentRegionAvail(); size.x=std::max(1.0f,size.x); size.y=std::max(1.0f,size.y);
    const ImVec2 end{origin.x+size.x,origin.y+size.y};
    ImGui::InvisibleButton("canvas",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered=ImGui::IsItemHovered();
    const bool held=ImGui::IsItemActive();
    const bool focused=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const auto mouse=ImGui::GetMousePos();
    auto& io=ImGui::GetIO();
    ensurePositions(topology);
    if (fitPending_ && size.x>80 && size.y>80) { fit(topology,size); fitPending_=false; }
    if (hovered && io.MouseWheel!=0) view_.zoomAt(view_.zoom*std::pow(1.15f,io.MouseWheel),mouse,origin);
    if (held && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        view_.pan.x+=io.MouseDelta.x; view_.pan.y+=io.MouseDelta.y;
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    bool delivered=false;
    if (ImGui::BeginDragDropTarget()) {
        if (const auto* payload=ImGui::AcceptDragDropPayload(payloadType)) {
            if (payload->DataSize==sizeof(int)) {
                const int type=*static_cast<const int*>(payload->Data);
                if (type>=0 && type<static_cast<int>(kns::deviceTypeNames.size())) {
                    try { createDevice(engine,static_cast<kns::DeviceType>(type),view_.toWorld(mouse,origin),selectedNode); }
                    catch (const std::exception& e) { error_=e.what(); }
                    placing_.reset(); tool_=Tool::Select; dragging_=-1; delivered=true; routeEndpoints_.reset();
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    std::vector<std::pair<float,float>> positions(topology.size());
    int hit=-1;
    for (int id=0;id<topology.size();++id) {
        const auto* node=topology.getNode(id);
        if (!node->getPosition()) continue;
        // Removed nodes retain their coordinates for packets already in flight.
        const auto p=view_.toScreen(nodePoint(*node),origin);
        positions[id]={p.x,p.y};
        if (node->isActive() && hovered && std::abs(mouse.x-p.x)<=40*view_.zoom && mouse.y>=p.y-36*view_.zoom && mouse.y<=p.y+53*view_.zoom) hit=id;
    }
    std::optional<std::uint64_t> hitLink;
    float nearest=7;
    if (hovered && hit<0) for (const auto& link:topology.getLinks()) {
        const auto a=positions[link->getA()], b=positions[link->getB()];
        const float distance=distanceToLink(mouse,{a.first,a.second},{b.first,b.second});
        if (distance<nearest) { nearest=distance; hitLink=link->getId(); }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !delivered) {
        error_.clear();
        if (placing_) {
            try { createDevice(engine,*placing_,view_.toWorld(mouse,origin),selectedNode); }
            catch (const std::exception& e) { error_=e.what(); }
            placing_.reset();
        } else if (tool_==Tool::Select) {
            selectedNode=hit; selectedLink_=hit<0 ? hitLink : std::nullopt;
            dragging_=hit;
            if (hit>=0) {
                const auto world=view_.toWorld(mouse,origin), point=nodePoint(*topology.getNode(hit));
                dragOffset_={point.x-world.x,point.y-world.y};
            }
        } else if (hit>=0) {
            selectedNode=hit; selectedLink_.reset();
            if (source_<0) { source_=hit; routeEndpoints_.reset(); }
            else if (hit!=source_ || tool_==Tool::Route) {
                if (tool_==Tool::TCP) connection=std::pair{source_,hit};
                else if (tool_==Tool::Route) {
                    routeEndpoints_=std::pair{source_,hit}; routeRevision_.reset();
                }
                else {
                    try {
                        const auto& links=topology.getLinksFromNode(source_);
                        if (std::any_of(links.begin(),links.end(),[&](const auto& l) { return l->getOtherNode(source_)==hit; })) {
                            throw std::invalid_argument("These devices are already connected. Edit the existing cable.");
                        }
                        selectedLink_=engine.createLink(source_,hit,cableOptions_.bandwidthMbps,
                            cableOptions_.delayMs,cableOptions_.lossPercent/100.0,
                            cableOptions_.mode,cableOptions_.queueCapacity)->getId();
                    } catch (const std::exception& e) { error_=e.what(); }
                }
                source_=-1;
            }
        }
    }
    if (dragging_>=0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && held) {
        auto world=view_.toWorld(mouse,origin);
        world=bounded({world.x+dragOffset_.x,world.y+dragOffset_.y},snap_);
        topology.setNodePosition(dragging_,{world.x,world.y});
        const auto p=view_.toScreen(world,origin); positions[dragging_]={p.x,p.y};
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging_=-1;
    if (focused && !io.WantTextInput && !ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { placing_.reset(); source_=-1; dragging_=-1; tool_=Tool::Select; routeEndpoints_.reset(); }
        remove=remove || ImGui::IsKeyPressed(ImGuiKey_Delete,false);
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        selectedNode=hit; selectedLink_=hit<0 ? hitLink : std::nullopt;
        if (hit>=0) std::snprintf(label_.data(),label_.size(),"%s",topology.getNode(hit)->getLabel().c_str());
        ImGui::OpenPopup("canvas-context");
    }
    ImGui::SetNextWindowSize({360,0},ImGuiCond_Always);
    if (ImGui::BeginPopup("canvas-context")) {
        if (active(selectedNode)) {
            ImGui::Text("%s #%d",tr.translate("Device").c_str(),selectedNode);
            if (ImGui::InputText(tr.translate("Label").c_str(),label_.data(),label_.size())) topology.setNodeLabel(selectedNode,label_.data());
            int type=static_cast<int>(topology.getNode(selectedNode)->getDeviceInfo().type);
            if (ImGui::BeginCombo(tr.translate("Type").c_str(),tr.translate(deviceDisplayName(static_cast<kns::DeviceType>(type))).c_str())) {
                for (int i=0;i<10;++i) if (ImGui::Selectable(tr.translate(deviceDisplayName(static_cast<kns::DeviceType>(i))).c_str(),i==type)) {
                    auto info=topology.getNode(selectedNode)->getDeviceInfo(); info.type=static_cast<kns::DeviceType>(i);
                    info.evidence="user_override"; topology.setNodeDeviceInfo(selectedNode,std::move(info));
                }
                ImGui::EndCombo();
            }
            if (ImGui::MenuItem(tr.translate("Connect cable from here").c_str())) { source_=selectedNode; tool_=Tool::Cable; placing_.reset(); routeEndpoints_.reset(); }
            if (ImGui::MenuItem(tr.translate("Start TCP from here").c_str())) { source_=selectedNode; tool_=Tool::TCP; placing_.reset(); routeEndpoints_.reset(); }
            if (ImGui::MenuItem(tr.translate("Inspect route from here").c_str())) { source_=selectedNode; tool_=Tool::Route; placing_.reset(); routeEndpoints_.reset(); }
            if (ImGui::MenuItem(tr.translate("Delete device").c_str())) remove=true;
        } else if (selectedLink_) {
            for (const auto& link:topology.getLinks()) if (link->getId()==*selectedLink_) {
                ImGui::Text("%d %s %d",link->getA(),link->getMode()==kns::LinkMode::SIMPLEX ? "->" : "<->",link->getB());
                double bandwidth=link->getBandwidthMbps(), delay=link->getDelayMs(),loss=link->getLossProb()*100;
                try {
                    if (ImGui::InputDouble("Mbps",&bandwidth,0,0,"%.1f")) link->setBandwidthMbps(bandwidth);
                    if (ImGui::InputDouble(tr.translate("Delay (ms)").c_str(),&delay,0,0,"%.2f")) link->setDelayMs(delay);
                    if (ImGui::InputDouble(tr.translate("Loss (%)").c_str(),&loss,0,0,"%.2f")) link->setLossProb(loss/100);
                } catch (const std::exception& e) { error_=e.what(); }
                int capacity=static_cast<int>(link->getQueueCapacity());
                if (ImGui::InputInt(tr.translate("Queue capacity").c_str(),&capacity,0,0)) {
                    try { link->setQueueCapacity(capacity); error_.clear(); }
                    catch (const std::exception& e) { error_=e.what(); }
                }
                auto mode=link->getMode();
                if (modePicker(mode,tr)) {
                    try { link->setMode(mode); error_.clear(); }
                    catch (const std::exception& e) { error_=e.what(); }
                }
                if (!error_.empty()) ImGui::TextWrapped("%s",error_.c_str());
                bool up=link->isUp(); if (ImGui::Checkbox(tr.translate("Up").c_str(),&up)) link->setUp(up);
                if (ImGui::MenuItem(tr.translate("Delete cable").c_str())) remove=true;
                break;
            }
        } else if (ImGui::MenuItem(tr.translate("Fit topology").c_str())) fitPending_=true;
        ImGui::EndPopup();
    }
    if (remove) {
        if (selectedLink_) { engine.deleteLinkById(*selectedLink_); selectedLink_.reset(); }
        else if (active(selectedNode)) { engine.deleteNode(selectedNode); selectedNode=-1; }
        source_=-1; dragging_=-1;
    }

    auto* draw=ImGui::GetWindowDrawList();
    draw->PushClipRect(origin,end,true);
    const bool dark=ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x<0.5f;
    const auto background=dark ? IM_COL32(19,28,40,255) : IM_COL32(240,245,250,255);
    const auto dots=dark ? IM_COL32(56,73,93,255) : IM_COL32(189,204,220,255);
    draw->AddRectFilled(origin,end,background);
    float step=grid*view_.zoom; while (step<14) step*=2;
    for (float x=origin.x+std::fmod(view_.pan.x,step);x<end.x;x+=step)
        for (float y=origin.y+std::fmod(view_.pan.y,step);y<end.y;y+=step) draw->AddCircleFilled({x,y},1,dots,4);
    std::set<std::pair<int,int>> busy;
    for (const auto& packet:packets) if (visualTime>=packet.visual_start_time &&
        visualTime<=packet.visual_start_time+packet.visual_duration) busy.insert(std::minmax(packet.from,packet.to));
    updateRoute(engine);
    std::set<std::uint64_t> routeLinks;
    if (routeEndpoints_) for (const auto& hop:route_.hops) routeLinks.insert(hop.link_id);
    for (const auto& link:topology.getLinks()) {
        const auto a=positions[link->getA()],b=positions[link->getB()];
        ImU32 color=link->isUp() ? IM_COL32(102,146,172,255) : IM_COL32(196,88,98,255);
        if (busy.contains(std::minmax(link->getA(),link->getB()))) color=IM_COL32(246,180,65,255);
        if (selectedLink_==link->getId()) color=IM_COL32(78,190,255,255);
        const bool onRoute=routeLinks.contains(link->getId());
        if (onRoute) color=IM_COL32(94,224,174,255);
        draw->AddLine({a.first,a.second},{b.first,b.second},color,onRoute || selectedLink_==link->getId() ? 4.0f : 2.0f);
        if (link->getMode()==kns::LinkMode::SIMPLEX) {
            const float dx=b.first-a.first,dy=b.second-a.second,len=std::hypot(dx,dy);
            if (len>1) {
                const ImVec2 mid{(a.first+b.first)*0.5f,(a.second+b.second)*0.5f};
                draw->AddTriangleFilled({mid.x+dx/len*7,mid.y+dy/len*7},
                    {mid.x-dx/len*7-dy/len*5,mid.y-dy/len*7+dx/len*5},
                    {mid.x-dx/len*7+dy/len*5,mid.y-dy/len*7-dx/len*5},color);
            }
        }
    }
    if (source_>=0 && active(source_)) {
        const auto point=positions[source_];
        draw->AddLine({point.first,point.second},mouse,IM_COL32(76,192,244,255),2);
    }
    int activeCount=0;
    for (int id=0;id<topology.size();++id) {
        if (!active(id)) continue;
        ++activeCount;
        // A just-created click placement gets its position on this frame too.
        const auto p=view_.toScreen(nodePoint(*topology.getNode(id)),origin);
        if (p.x+80*view_.zoom<origin.x || p.x-80*view_.zoom>end.x || p.y+70*view_.zoom<origin.y || p.y-45*view_.zoom>end.y) continue;
        const float s=view_.zoom;
        const ImVec2 lo{p.x-37*s,p.y-34*s},hi{p.x+37*s,p.y+34*s};
        draw->AddRectFilled({lo.x+3*s,lo.y+5*s},{hi.x+3*s,hi.y+5*s},IM_COL32(0,0,0,35),10*s);
        draw->AddRectFilled(lo,hi,dark ? IM_COL32(31,46,64,255) : IM_COL32(255,255,255,255),10*s);
        draw->AddRect(lo,hi,id==selectedNode || id==source_ ? IM_COL32(71,188,255,255) : dots,10*s,0,id==selectedNode ? 2.5f : 1);
        drawDeviceIcon(draw,topology.getNode(id)->getDeviceInfo().type,p,51*s);
        if (s>=0.38f) {
            const auto& node=*topology.getNode(id);
            const std::string label=node.getLabel().empty() ? deviceDisplayName(node.getDeviceInfo().type) : node.getLabel();
            const float font=std::clamp(12*s,9.0f,18.0f);
            // Clip long hostnames instead of allowing them to overlap neighbors.
            draw->PushClipRect({p.x-83*s,p.y+37*s},{p.x+83*s,p.y+68*s},true);
            const auto text=ImGui::GetFont()->CalcTextSizeA(font,10000,0,label.c_str());
            draw->AddText(ImGui::GetFont(),font,{p.x-std::min(text.x,160*s)/2,p.y+38*s},ImGui::GetColorU32(ImGuiCol_Text),label.c_str());
            const auto number="#"+std::to_string(id);
            const auto numberSize=ImGui::GetFont()->CalcTextSizeA(10*s,1000,0,number.c_str());
            draw->AddText(ImGui::GetFont(),10*s,{p.x-numberSize.x/2,p.y+53*s},ImGui::GetColorU32(ImGuiCol_TextDisabled),number.c_str());
            draw->PopClipRect();
        }
    }
    if (positions.size()==static_cast<std::size_t>(topology.size())) {
        PacketRenderer{}.render(draw,positions,packets,visualTime,tr);
    }
    if (placing_ && hovered) drawDeviceIcon(draw,*placing_,mouse,51*view_.zoom);
    if (!activeCount) {
        const auto hint=tr.translate("Build your network\nDrag devices from the palette, then use Cable to connect them.");
        draw->AddText({origin.x+24,origin.y+32},ImGui::GetColorU32(ImGuiCol_TextDisabled),hint.c_str());
    }
    draw->PopClipRect();
    if (hovered && hit>=0 && !held && !ImGui::IsPopupOpen("canvas-context")) {
        const auto* node=topology.getNode(hit);
        ImGui::BeginTooltip();
        ImGui::Text("%s (#%d)",node->getLabel().c_str(),hit);
        ImGui::TextUnformatted(tr.translate(deviceDisplayName(node->getDeviceInfo().type)).c_str());
        for (const auto& address:node->getDeviceInfo().addresses) ImGui::TextUnformatted(address.c_str());
        ImGui::EndTooltip();
    }
    ImGui::EndChild(); ImGui::End();
    return connection;
}
}
