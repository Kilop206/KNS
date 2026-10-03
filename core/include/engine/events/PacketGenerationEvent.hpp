#pragma once

<<<<<<< HEAD
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
=======
#include <cstdint>

#include "engine/core/Event.hpp"

namespace kns {

    class PacketGenerationEvent : public Event {
        public:
            PacketGenerationEvent(
                double timestamp,
                int source,
                int destination,
                std::uint64_t session_id
            );

            void execute(SimulationEngine& engine) override;
            const char* getName() const noexcept override { return "PacketGenerationEvent"; }

        private:
            int source_;
            int destination_;
            std::uint64_t session_id_;
    };
>>>>>>> 879e9a30eb706359e007b3218a4c881c257cd5bc

}
