#pragma once

#include <cstdint>

#include "engine/events/Event.hpp"

namespace kns {

    class SimulationEngine;

    class TCPAckEvent : public Event {
        private:
            uint32_t seq_num;
            uint32_t ack_num;
            int source_;
            int destination_;
            uint64_t session_id;

        public:
        TCPAckEvent(double timestamp,
            int source,
            int destination,
            uint32_t seq_num,
            uint32_t ack_num,
            uint64_t session_id);

        void execute(SimulationEngine& engine) override;
    };
}