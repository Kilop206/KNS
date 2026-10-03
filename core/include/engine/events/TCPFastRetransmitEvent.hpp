#pragma once

#include <cstdint>

#include "engine/core/Event.hpp"

namespace kns {

    class TCPFastRetransmitEvent : public Event {
    public:
        TCPFastRetransmitEvent(
            double timestamp,
            std::uint64_t session_id
        );

        void execute(
            SimulationEngine& engine
        ) override;

        const char* getName() const noexcept override {
            return "TCPFastRetransmitEvent";
        }

    private:
        std::uint64_t session_id_;
    };

} // namespace kns