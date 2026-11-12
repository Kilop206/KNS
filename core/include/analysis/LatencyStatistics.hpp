#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <array>
#include <nlohmann/json.hpp>

namespace kns::analysis {
    class LatencyStatistics {
    public:
        explicit LatencyStatistics(std::size_t retention = 4096);
        void add(double seconds);
        nlohmann::json toJson() const;
    private:
        std::size_t retention_;
        std::uint64_t count_ = 0;
        long double mean_ = 0, m2_ = 0;
        double min_ = 0, max_ = 0;
        std::deque<double> recent_;
        std::array<std::uint64_t, 8> histogram_{};
    };
}
