#pragma once
#include <cstdint>

namespace kns::tcp_sequence {
    inline constexpr std::uint32_t half_space = std::uint32_t{1} << 31;
    constexpr std::uint32_t distance(std::uint32_t from, std::uint32_t to) noexcept {
        return to - from;
    }
    constexpr bool before(std::uint32_t a, std::uint32_t b) noexcept {
        const auto delta = distance(a, b);
        return delta != 0 && delta < half_space;
    }
    constexpr bool beforeOrEqual(std::uint32_t a, std::uint32_t b) noexcept {
        return a == b || before(a, b);
    }
}
