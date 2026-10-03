#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"

namespace kns
{
    class TCPDelayedAckEvent : public Event
    {
    public:
        static constexpr double DEFAULT_DELAY = 0.2;

        TCPDelayedAckEvent(
            double timestamp,
            std::uint64_t session_id,
            int receiver_node,
            std::uint32_t acknowledgement
        );

        void execute(
            SimulationEngine& engine
        ) override;

        const char* getName() const noexcept override
        {
            return "TCPDelayedAckEvent";
        }

    private:
        std::uint64_t session_id_;
        int receiver_node_;
        std::uint32_t acknowledgement_;
    };
}