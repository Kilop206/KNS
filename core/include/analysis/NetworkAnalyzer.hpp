#pragma once

#include "analysis/NetworkAnalysis.hpp"

namespace kns {

class Topology;

}

namespace kns::analysis {

class NetworkAnalyzer {
public:
    [[nodiscard]]
    NetworkAnalysis analyze(const Topology& topology) const;
};

}