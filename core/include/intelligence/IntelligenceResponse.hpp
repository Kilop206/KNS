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

enum class IntelligenceStatus {
    Completed,
    Failed,
    Partial
};

struct IntelligenceAffectedLink {
    int from = -1;
    int to = -1;
};

struct IntelligenceFinding {
    std::string id;

    FindingSeverity severity =
        FindingSeverity::Info;

    std::string category;

    std::string title;
    std::string description;

    std::vector<int> affected_nodes;

    std::vector<IntelligenceAffectedLink>
        affected_links;
};

struct IntelligenceRecommendation {
    std::string id;

    std::string priority;

    std::string title;
    std::string description;
};

struct IntelligenceResponse {
    std::string schema_version;

    std::string analysis_id;

    IntelligenceStatus status =
        IntelligenceStatus::Completed;

    double network_score = 0.0;

    std::string summary;

    std::vector<IntelligenceFinding>
        findings;

    std::vector<IntelligenceRecommendation>
        recommendations;
};

}