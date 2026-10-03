#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"

namespace kns {

    class TCPTimeoutEvent : public Event {
    public:
        TCPTimeoutEvent(
            double timestamp,
            std::uint64_t session_id,
            std::uint32_t sequence
        );

        void execute(
            SimulationEngine& engine
        ) override;

        const char* getName() const noexcept override {
            return "TCPTimeoutEvent";
        }

    private:
        std::uint64_t session_id_;
        std::uint32_t sequence_;
    };

} // namespace kns