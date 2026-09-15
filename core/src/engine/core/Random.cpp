#include "engine/core/Random.hpp"

#include <random>

namespace kns {

    void Random::seed(std::uint64_t seed) {
        engine_.seed(seed);
    }

    double Random::uniform01() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(engine_);
    }

    std::uint32_t Random::nextUint32() {
        return static_cast<std::uint32_t>(engine_());
    }

}
