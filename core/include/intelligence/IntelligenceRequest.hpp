#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace kns::intelligence {

enum class AnalysisMode {
    Quick,
    Detailed,
    Deep
};

struct IntelligenceRequest {
    std::string request_id;

    AnalysisMode mode =
        AnalysisMode::Detailed;

    nlohmann::json context;

    std::string client_name = "KNS";
    std::string client_version = "1.0.0";
};

}