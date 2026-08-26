#pragma once

#include "engine/core/Event.hpp"

namespace kns {

    class TCPConnectionCloseEvent : public Event
    {
        public:
            TCPConnectionCloseEvent(
                double timestamp,
                std::uint64_t sessionId
            );

            void execute(SimulationEngine& engine) override;
            const char* getName() const noexcept override { return "TCPConnectionCloseEvent"; }

        private:
            std::uint64_t session_id_;
        };

}
