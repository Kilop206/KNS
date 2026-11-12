#pragma once

#include <memory>
#include <nlohmann/json.hpp>

namespace kns {
    class SimulationEngine;

    /// Single-owner-thread log; scheduled events retain it until dispatch.
    struct FaultReport {
        nlohmann::json plan;
        nlohmann::json changes = nlohmann::json::array();
        nlohmann::json observations = nlohmann::json::array();
        nlohmann::json toJson() const;
    };

    class FaultLab {
    public:
        /// Validate the entire plan before scheduling anything. Link indices
        /// resolve to stable IDs at scheduling, including parallel links.
        static std::shared_ptr<FaultReport> schedule(SimulationEngine&, const nlohmann::json& plan);
    };
}
