#pragma once

#include <cstdint>

namespace kns {

    // Global deterministic random source for the simulation.
    // Seed it once (e.g. from RunConfig::seed) to make runs reproducible.
    class Random {
        public:
            static void seed(std::uint64_t seed);

            // Uniform double in [0, 1).
            static double uniform01();

            static std::uint32_t nextUint32();
    };

}
