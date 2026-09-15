#pragma once

#include "analysis/NetworkAnalysis.hpp"
#include "analysis/NetworkScore.hpp"

namespace kns::analysis {

class NetworkScoreCalculator {
public:
    [[nodiscard]]
    static NetworkScore calculate(
        const NetworkAnalysis& analysis
    );
};

}