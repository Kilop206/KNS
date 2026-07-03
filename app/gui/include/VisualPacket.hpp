#pragma once

#include <cstdint>

#include "../../../core/include/enums/PacketType.hpp"

namespace gui {
    struct VisualPacket {
        int from = -1;
        int to = -1;
        kns::PacketType type = kns::PacketType::DATA;
        uint64_t session_id = 0;

        double sim_departure_time = 0.0;
        double sim_arrival_time = 0.0;

        double visual_start_time = 0.0;
        double visual_duration = 0.35;
    };
}