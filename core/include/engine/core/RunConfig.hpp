#pragma once

#include <string>

namespace kns {
    struct RunConfig {
        std::string filename;
        int seed;
        int packet_size = 1500;
    };
}