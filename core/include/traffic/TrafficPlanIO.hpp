#pragma once

#include <cstddef>
#include <iosfwd>
#include <string_view>
#include <nlohmann/json.hpp>

namespace kns {
    class SimulationEngine;
}

namespace kns::traffic {
    inline constexpr std::size_t MAX_PLAN_BYTES = 4 * 1024 * 1024;
    inline constexpr int MAX_PLAN_DEPTH = 32;

    /// Strict bounded JSON input and native semantic preflight. Neither call schedules work.
    /// Duplicate object keys and excessive nesting are rejected, including unknown fields.
    nlohmann::json parseTrafficPlan(std::string_view text, const SimulationEngine& engine);
    nlohmann::json readTrafficPlan(std::istream& input, const SimulationEngine& engine);
}
