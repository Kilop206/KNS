#pragma once

#include <nlohmann/json.hpp>

#include "analysis/NetworkAnalysis.hpp"

namespace kns::analysis {

class AnalysisJsonSerializer {
public:
    [[nodiscard]]
    static nlohmann::json toJson(
        const NetworkAnalysis& analysis
    );
};

}