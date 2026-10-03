#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "intelligence/IntelligenceResponse.hpp"

namespace kns::intelligence {

class IntelligenceResponseParser {
public:
    [[nodiscard]]
    static IntelligenceResponse parse(
        const nlohmann::json& json
    );

    [[nodiscard]]
    static IntelligenceResponse parse(
        const std::string& jsonText
    );
};

}