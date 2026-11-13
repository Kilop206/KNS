#pragma once
#include <memory>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace kns { class SimulationEngine; class Link; struct Packet; }
namespace kns::analysis {
    class MetricsCollector {
    public:
        static std::shared_ptr<MetricsCollector> attach(SimulationEngine&, const nlohmann::json&);
        ~MetricsCollector();
        void transmission(const Packet&, const Link&, double admitted, double start, double end);
        void delivery(const Packet&, double now);
        void tcpPayloadReleased(const Packet&, std::uint32_t bytes, double now);
        void sample(SimulationEngine&);
        nlohmann::json toJson() const;
    private:
        struct Data;
        explicit MetricsCollector(const nlohmann::json&);
        std::unique_ptr<Data> data_;
    };
}
