#pragma once

#include <string>
#include <utility>
#include <vector>

namespace kns::intelligence {

enum class FindingSeverity {
    Info,
    Low,
    Medium,
    High,
    Critical
};

struct IntelligenceFinding {
    std::string title;
    std::string description;

    FindingSeverity severity =
        FindingSeverity::Info;

    std::vector<int> affected_nodes;

    std::vector<std::pair<int, int>>
        affected_links;
};

struct IntelligenceRecommendation {
    std::string title;
    std::string description;

    std::string priority;
};

struct IntelligenceResponse {
    std::string analysis_id;

    int network_score = 0;

    std::string summary;

    std::vector<IntelligenceFinding>
        findings;

    std::vector<IntelligenceRecommendation>
        recommendations;
};

}