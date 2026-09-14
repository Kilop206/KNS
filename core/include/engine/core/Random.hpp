#pragma once

#include <cstdint>
#include <random>

namespace kns {

    // Value-owned deterministic stream; separate owners never share state.
    class Random {
        std::mt19937_64 engine_{42};
        public:
            void seed(std::uint64_t seed);

            // Uniform double in [0, 1).
            double uniform01();

            std::uint32_t nextUint32();
    };

}
