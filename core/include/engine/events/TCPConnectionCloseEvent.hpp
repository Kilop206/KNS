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

        private:
            std::uint64_t session_id_;
        };

}