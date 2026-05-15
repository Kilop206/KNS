#pragma once

#include "engine/events/Event.hpp"

#include <cstdint>

namespace kns {

    class SimulationEngine;

    class TCPHandshakeEvent : public Event {
    public:
        TCPHandshakeEvent(double timestamp, int source, int destination);

        void execute(SimulationEngine& engine) override;

    private:
        int source_;
        int destination_;
        uint32_t seq_num;
        uint32_t ack_num;
    };

}