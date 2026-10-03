#pragma once

#include <cstddef>

#include <nlohmann/json.hpp>

#include "analysis/NetworkAnalysis.hpp"

namespace kns::analysis {

enum class AIContextDepth {
    Quick,
    Detailed,
    Deep
};

struct AIContextOptions {
    AIContextDepth depth = AIContextDepth::Detailed;

    std::size_t max_critical_nodes = 5;
    std::size_t max_critical_links = 5;
    std::size_t max_worst_routes = 5;
    std::size_t max_unreachable_routes = 10;
};

class AIContextBuilder {
public:
    [[nodiscard]]
    static nlohmann::json build(
        const NetworkAnalysis& analysis,
        const AIContextOptions& options = {}
    );
};

}