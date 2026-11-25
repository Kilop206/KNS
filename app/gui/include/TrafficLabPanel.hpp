#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace kns {
    class SimulationEngine;
    namespace traffic {
        class TrafficRun;
    }
}

namespace gui {
    class TranslationService;
    enum class TrafficPattern { Periodic, CBR, Burst, Poisson };

    struct TrafficFlowDraft {
        int source = 0, destination = 1, wireBytes = 1000;
        double start = 0, duration = 1;
        TrafficPattern pattern = TrafficPattern::Periodic;
        double intervalMs = 100, rateMbps = 1, ratePps = 100;
        int burstPackets = 10;
        double burstIntervalMs = 250, packetIntervalMs = 1;
        bool deadline = false;
        double deadlineMs = 10;

        nlohmann::json toJson(std::uint32_t id) const;
    };

    class TrafficLabPanel {
    public:
        /// Returns true only when a new workload was scheduled. Caller pauses the GUI clock.
        bool render(kns::SimulationEngine&, TranslationService&, std::uint64_t networkSeed);
        std::optional<nlohmann::json> report(const kns::SimulationEngine&) const;

    private:
        std::vector<TrafficFlowDraft> flows_{TrafficFlowDraft{}};
        int selected_ = 0, maximum_ = 10000;
        std::uint64_t seed_ = 42, networkSeed_ = 42;
        const kns::SimulationEngine* engine_ = nullptr;
        std::shared_ptr<kns::traffic::TrafficRun> run_;
        std::optional<nlohmann::json> importedPlan_;
        std::string error_;

        nlohmann::json plan() const;
    };
}
