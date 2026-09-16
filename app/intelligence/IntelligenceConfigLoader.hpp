#pragma once

#include "intelligence/IntelligenceClient.hpp"

namespace kns::app::intelligence {

class IntelligenceConfigLoader {
public:
    [[nodiscard]]
    static IntelligenceClientConfig fromEnvironment();
};

}