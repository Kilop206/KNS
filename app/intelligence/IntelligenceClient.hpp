#pragma once

#include <string>

#include "intelligence/IntelligenceRequest.hpp"
#include "intelligence/IntelligenceResponse.hpp"

namespace kns::app::intelligence {

struct IntelligenceClientConfig {
    std::string base_url =
        "http://localhost:8080";

    std::string analyze_endpoint =
        "/api/v1/intelligence/analyze";

    std::string bearer_token;

    int connection_timeout_seconds = 5;
    int read_timeout_seconds = 30;
    int write_timeout_seconds = 10;
};

class IntelligenceClient {
public:
    explicit IntelligenceClient(
        IntelligenceClientConfig config = {}
    );

    [[nodiscard]]
    kns::intelligence::IntelligenceResponse analyze(
        const kns::intelligence::IntelligenceRequest& request
    ) const;

private:
    IntelligenceClientConfig config_;
};

}