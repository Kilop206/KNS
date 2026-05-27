#pragma once

#include <cstdint>

#include "engine/events/Event.hpp"

namespace kns {

    class SimulationEngine;

    class TCPFinEvent : public Event {
        private:
            uint32_t seq_num;
            uint32_t ack_num;
            int source_;
            int destination_;
            uint64_t session_id;

        public:
        TCPFinEvent(double timestamp,
            int source,
            int destination,
            uint32_t seq_num,
            uint32_t ack_num,
            uint64_t session_id);

        void execute(SimulationEngine& engine) override;
    };
}