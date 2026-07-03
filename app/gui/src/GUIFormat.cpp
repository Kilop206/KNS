#include "../include/GUIFormat.hpp"

#include <sstream>
#include <iomanip>
#include <cstdint>
#include <string>

namespace gui {

    std::string formatBytes(std::uint64_t bytes) {
        const char* suffix[] = {"B", "KB", "MB", "GB", "TB"};
        double value = static_cast<double>(bytes);
        int i = 0;

        while (value >= 1024.0 && i < 4) {
            value /= 1024.0;
            ++i;
        }

        std::ostringstream oss;
        if (i == 0) {
            oss << static_cast<std::uint64_t>(value) << " " << suffix[i];
        } else {
            oss << std::fixed << std::setprecision(1) << value << " " << suffix[i];
        }
        return oss.str();
    }

    std::string formatPercent(double value01) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << value01 * 100.0 << "%";
        return oss.str();
    }

}