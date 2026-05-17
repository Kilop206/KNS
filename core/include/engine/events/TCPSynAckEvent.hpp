#pragma once

#include <cstdint>

#include "engine/events/Event.hpp"

namespace kns {

    class SimulationEngine;

    class TCPSynAckEvent : public Event {
        private:
            uint32_t seq_num;
            uint32_t ack_num;
            int source_;
            int destination_;

        public:
        TCPSynAckEvent(double timestamp,
            int source,
            int destination,
            uint32_t seq_num,
            uint32_t ack_num);

        void execute(SimulationEngine& engine) override;
    };
}