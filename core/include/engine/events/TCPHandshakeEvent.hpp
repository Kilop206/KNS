#pragma once

#include "Event.hpp"
#include "../../network/tcp/TCPConnection.hpp"

namespace kns {
    class TCPHandshakeEvent : public Event {
        public:
            void execute(TCPConnection conn);
    };
}