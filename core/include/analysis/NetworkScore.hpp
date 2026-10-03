#pragma once

namespace kns::analysis {

struct NetworkScore {
    double overall_score = 0.0;

    double resilience_score = 0.0;
    double routing_score = 0.0;
    double performance_score = 0.0;
    double risk_score = 0.0;
};

}