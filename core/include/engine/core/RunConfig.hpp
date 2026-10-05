#pragma once

#include <string>
#include <cstdint>

#include "network/transport/tcp/congestion/CongestionControlType.hpp"

namespace kns {
    struct RunConfig {
        std::string filename;
        std::uint64_t seed = 42;
        int packet_size = 1500;
        bool auto_start = true;
        CongestionControlType congestion_control = CongestionControlType::RENO;
    };
}
