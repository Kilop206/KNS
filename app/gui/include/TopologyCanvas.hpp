#pragma once
#include "CanvasView.hpp"
#include "VisualPacket.hpp"
#include "network/DeviceType.hpp"
#include "network/RouteTrace.hpp"
#include "network/Routing.hpp"
#include "enums/LinkMode.hpp"
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kns { class SimulationEngine; class Topology; }
namespace gui {
class TranslationService;

class TopologyCanvas {
public:
    // Returns a requested TCP connection; moving devices never creates traffic.
    std::optional<std::pair<int,int>> render(kns::SimulationEngine& engine, int& selectedNode,
        const std::vector<VisualPacket>& packets, double visualTime, TranslationService& translations);
    void reset();
private:
    enum class Tool { Select, Cable, TCP, Route };
    CanvasView view_;
    Tool tool_ = Tool::Select;
    std::optional<kns::DeviceType> placing_;
    std::optional<std::uint64_t> selectedLink_;
    int source_ = -1;
    std::optional<std::pair<int,int>> routeEndpoints_;
    kns::RouteTrace route_;
    std::optional<std::uint64_t> routeRevision_;
    kns::RoutingMetric routeMetric_ = kns::RoutingMetric::Delay;
    int dragging_ = -1;
    ImVec2 dragOffset_{};
    bool snap_ = true;
    bool fitPending_ = true;
    std::string error_;
    std::array<char, 256> label_{};
    struct CableOptions {
        double bandwidthMbps = 100.0;
        double delayMs = 1.0;
        double lossPercent = 0.0;
        int queueCapacity = 32;
        kns::LinkMode mode = kns::LinkMode::FULL_DUPLEX;
    } cableOptions_;
    void ensurePositions(kns::Topology& topology);
    void arrange(kns::Topology& topology);
    void fit(const kns::Topology& topology, ImVec2 size);
    void palette(TranslationService& translations);
    void cableSettings(TranslationService& translations);
    void updateRoute(const kns::SimulationEngine& engine);
    void createDevice(kns::SimulationEngine& engine, kns::DeviceType type, ImVec2 position, int& selectedNode);
};
}
