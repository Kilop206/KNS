#pragma once

#include <nlohmann/json.hpp>

#include "analysis/NetworkAnalysis.hpp"
#include "intelligence/IntelligenceRequest.hpp"

namespace kns::intelligence {

class IntelligenceRequestBuilder {
public:
    [[nodiscard]]
    static IntelligenceRequest build(
        const analysis::NetworkAnalysis& analysis,
        AnalysisMode mode =
            AnalysisMode::Detailed
    );

    [[nodiscard]]
    static nlohmann::json toJson(
        const IntelligenceRequest& request
    );
};

}