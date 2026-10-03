#pragma once

#include <cstdint>
#include <memory>

#include "network/transport/tcp/congestion/CongestionControl.hpp"
#include "network/transport/tcp/congestion/CongestionControlType.hpp"

namespace kns
{
    class CongestionControlFactory
    {
    public:
        static std::unique_ptr<CongestionControl> create(
            CongestionControlType type,
            std::uint32_t mss,
            std::uint32_t initial_ssthresh = 65535
        );
    };
}