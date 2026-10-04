#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

#include "network/DiscoveryDiff.hpp"

namespace kns {

class DiscoveryDiffWatcher {
public:
    void setSource(std::string path, bool enabled);
    std::optional<DiscoveryDiff> poll(bool force = false);
    const std::string& error() const noexcept { return error_; }
    bool busy() const noexcept { return pending_.valid(); }
    std::uint64_t completedChecks() const noexcept { return completed_checks_; }

private:
    struct Result {
        std::string contents;
        std::optional<DiscoveryDiff> diff;
        std::string error;
    };

    std::string path_;
    std::string contents_;
    std::string error_;
    bool enabled_ = false;
    std::uint64_t generation_ = 0;
    std::uint64_t pending_generation_ = 0;
    std::uint64_t completed_checks_ = 0;
    std::future<Result> pending_;
    std::chrono::steady_clock::time_point next_check_{};
};

} // namespace kns
