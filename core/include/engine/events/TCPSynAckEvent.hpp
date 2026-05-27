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
            std::uint64_t session_id;

        public:
        TCPSynAckEvent(double timestamp,
            int source,
            int destination,
            uint32_t seq_num,
            uint32_t ack_num,
            uint64_t session_id);

        void execute(SimulationEngine& engine) override;
    };
}