#pragma once

#include <cstdint>

namespace kns
{
    enum class CongestionControlType : std::uint8_t
    {
        TAHOE,
        RENO,
        NEW_RENO,
        CUBIC
    };
}