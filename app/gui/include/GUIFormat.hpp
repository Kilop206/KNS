#pragma once

#include <cstdint>
#include <string>

namespace gui {
    std::string formatBytes(std::uint64_t bytes);
    std::string formatPercent(double value01);
}