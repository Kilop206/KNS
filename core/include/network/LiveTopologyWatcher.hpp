#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

#include "network/Topology.hpp"

namespace kns {

/// Reads and validates files on a worker. The caller applies returned snapshots
/// on the simulation thread. Changing source discards any late worker result.
class LiveTopologyWatcher {
public:
    void setSource(std::string path, bool enabled);
    std::optional<Topology> poll(bool force = false);
    const std::string& error() const noexcept { return error_; }
    bool busy() const noexcept { return pending_.valid(); }
    std::uint64_t completedChecks() const noexcept { return completed_checks_; }

private:
    struct Result {
        std::string contents;
        std::optional<Topology> topology;
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
