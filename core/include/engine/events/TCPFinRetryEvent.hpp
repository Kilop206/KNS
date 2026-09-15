#pragma once
#include "engine/core/Event.hpp"

namespace kns {
    class TCPFinRetryEvent final : public Event {
        std::uint64_t session_id_;
        int node_;
        unsigned retries_;
    public:
        static constexpr double INTERVAL = 1.0;
        static constexpr unsigned MAX_RETRIES = 5;
        static constexpr double TIME_WAIT_DURATION = 7.0;
        TCPFinRetryEvent(double time, std::uint64_t session, int node, unsigned retries = 0)
            : Event(time), session_id_(session), node_(node), retries_(retries) {}
        void execute(SimulationEngine& engine) override;
    };
}
