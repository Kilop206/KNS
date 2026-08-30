#pragma once

#include <cstdint>
#include "enums/PacketType.hpp"

namespace kns {
    struct PacketTravelInfo {
        double departure_time;
        double arrival_time;
        int from_node;
        int to_node;
        PacketType packet_type;

        /// Stable identity of the Link used for this transmission.
        /// Set by sendPacket(). Used by releasePacketThroughTopology() to
        /// release exactly the right link even when parallel links exist.
        std::uint64_t link_id = 0;
    };
}