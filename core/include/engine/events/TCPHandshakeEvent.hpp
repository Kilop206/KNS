#pragma once

<<<<<<< HEAD
#include "engine/events/Event.hpp"

namespace kns {

class TCPHandshakeEvent : public Event {
public:
    TCPHandshakeEvent(double timestamp, int source, int destination);

    void execute(SimulationEngine& engine) override;

private:
    int source_;
    int destination_;
};

}
=======
#include "engine/core/Event.hpp"

#include <cstdint>

namespace kns {

    class TCPHandshakeEvent : public Event {
        public:
            TCPHandshakeEvent(
                double timestamp,
                int source,
                int destination,
                std::uint64_t session_id
            );

            void execute(SimulationEngine& engine) override;
            const char* getName() const noexcept override { return "TCPHandshakeEvent"; }

        private:
            int source_;
            int destination_;
            std::uint64_t session_id_;
    };

}
>>>>>>> 879e9a30eb706359e007b3218a4c881c257cd5bc
