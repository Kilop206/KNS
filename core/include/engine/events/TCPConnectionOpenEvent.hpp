#pragma once

#include "core/SimulationEngine.hpp"
#include "core/Event.hpp"

namespace kns
{
    class TCPConnectionOpenEvent : public Event {
    private:
        std::uint64_t session_id;

    public:
        TCPConnectionOpenEvent(
            double timestamp,
            std::uint64_t session_id
        );

        void execute(SimulationEngine& engine) override;
    };
}