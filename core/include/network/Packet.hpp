#pragma once

#include "enums/PacketType.hpp"

#include <cstddef>

namespace kns {
    class Packet {
    public:
        const int source;
        const int destination;
        int current_node;
        double creation_time;
        double departure_time;
        std::size_t packet_size_bytes;
        int hop_count = 0;
        PacketType packet_type = PacketType::DATA;
        int seq_num = 0;
        int ack_num = 0;

        Packet(
            int source,
            int destination,
            int current_node,
            double creation_time,
            std::size_t packet_size_bytes
        )
            : source(source),
              destination(destination),
              current_node(current_node),
              creation_time(creation_time),
              departure_time(0.0),
              packet_size_bytes(packet_size_bytes) {}
    };
}