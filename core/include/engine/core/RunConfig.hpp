#pragma once

#include <string>
#include <cstdint>

namespace kns {
    struct RunConfig {
        std::string filename;
        std::uint64_t seed = 42;
        int packet_size = 1500;
        bool auto_start = true;
    };
}
