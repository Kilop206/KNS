#pragma once

#include <cstddef>
#include <cstdint>

namespace kns {
    enum class ExecutionState { Ready, Paused, Cancelled, Failed };
    enum class StopReason { QueueEmpty, TimeLimit, EventBudget, Paused, Cancelled, Failed };

    struct RunResult {
        StopReason reason;
        std::uint64_t events_processed;
        double simulation_time_s;
        std::size_t pending_events;
    };

    struct ExecutionStatistics {
        std::uint64_t events_dispatched = 0;
        std::uint64_t events_completed = 0;
        std::uint64_t events_failed = 0;
    };
}
