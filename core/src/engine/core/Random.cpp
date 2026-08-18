#include "engine/core/Random.hpp"

#include <random>

namespace kns {

    namespace {
        constexpr std::uint64_t kDefaultSeed = 42;

        std::mt19937_64& engine() {
            static std::mt19937_64 rng(kDefaultSeed);
            return rng;
        }
    }

    void Random::seed(std::uint64_t seed) {
        engine().seed(seed);
    }

    double Random::uniform01() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(engine());
    }

    std::uint32_t Random::nextUint32() {
        return static_cast<std::uint32_t>(engine()());
    }

}
