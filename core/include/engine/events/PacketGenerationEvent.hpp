#pragma once

#include "engine/events/Event.hpp"
#include "enums/PacketType.hpp"

namespace kns {

class PacketGenerationEvent : public Event {
public:
    PacketGenerationEvent(
        double timestamp,
        int source,
        int destination,
        PacketType type = PacketType::DATA
    );

    void execute(SimulationEngine& engine) override;

private:
    int source_;
    int destination_;
    PacketType type_;
};

}