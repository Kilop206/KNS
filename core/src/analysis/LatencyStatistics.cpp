#include "analysis/LatencyStatistics.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace kns::analysis {
namespace { constexpr std::array<double, 7> edges{0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0}; }
LatencyStatistics::LatencyStatistics(std::size_t retention) : retention_(retention) {
    if (retention == 0 || retention > 100000) throw std::invalid_argument("Latency retention must be in [1,100000]");
}
void LatencyStatistics::add(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Latency must be finite and nonnegative");
    const long double difference = seconds - mean_;
    const long double nextMean = mean_ + difference / static_cast<long double>(count_ + 1);
    const long double nextM2 = m2_ + difference * (seconds - nextMean);
    if (!std::isfinite(nextMean) || !std::isfinite(nextM2)) throw std::overflow_error("Latency accumulator overflow");
    if (recent_.size() == retention_) recent_.pop_front();
    recent_.push_back(seconds);
    min_ = count_ ? std::min(min_, seconds) : seconds;
    max_ = count_ ? std::max(max_, seconds) : seconds;
    mean_ = nextMean; m2_ = nextM2; ++count_;
    ++histogram_[std::upper_bound(edges.begin(), edges.end(), seconds) - edges.begin()];
}
nlohmann::json LatencyStatistics::toJson() const {
    using Json = nlohmann::json;
    std::vector<double> sorted(recent_.begin(), recent_.end());
    std::sort(sorted.begin(), sorted.end());
    const auto percentile = [&](double p) -> Json {
        if (sorted.empty()) return nullptr;
        const double position = p * static_cast<double>(sorted.size() - 1);
        const auto low = static_cast<std::size_t>(std::floor(position));
        const auto high = static_cast<std::size_t>(std::ceil(position));
        return sorted[low] + (sorted[high] - sorted[low]) * (position - static_cast<double>(low));
    };
    return {{"count", count_}, {"mean_s", count_ ? Json(static_cast<double>(mean_)) : Json(nullptr)},
        {"min_s", count_ ? Json(min_) : Json(nullptr)}, {"max_s", count_ ? Json(max_) : Json(nullptr)},
        {"sample_stddev_s", count_ > 1 ? Json(static_cast<double>(std::sqrt(std::max(0.0L, m2_) / (count_ - 1)))) : Json(nullptr)},
        {"retained_samples", sorted.size()}, {"discarded_samples", count_ - sorted.size()},
        {"percentile_scope", count_ > sorted.size() ? "most_recent_retained_deliveries" : "all_observed_deliveries"},
        {"median_s", percentile(0.5)}, {"p95_s", percentile(0.95)}, {"p99_s", percentile(0.99)},
        {"histogram", {{"upper_edges_s", edges}, {"counts", histogram_}, {"scope", "all_observed_deliveries"},
            {"intervals", "[lower,upper), final bin >= last edge"}}}};
}
}
