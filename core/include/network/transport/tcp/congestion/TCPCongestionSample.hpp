#pragma once

#include <cstdint>

namespace kns {

struct TcpCongestionSample {
    double timestamp;
    std::uint32_t cwnd;
    std::uint32_t ssthresh;
};

}