#pragma once

#include "engine/core/Event.hpp"

namespace kns {

class TCPHandshakeTimeoutEvent : public Event {
private:
    uint64_t session_id_;

public:
    TCPHandshakeTimeoutEvent(
        double time,
        uint64_t session_id
    );

    void execute(SimulationEngine& engine) override;
};

}