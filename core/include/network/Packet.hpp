#pragma once

#include <cstddef>
#include <cstdint>

#include "enums/PacketType.hpp"
#include "network/transport/tcp/TCPSegment.hpp"

namespace kns {

    inline PacketType inferPacketType(const TCPSegment& tcp)
    {
        if (tcp.syn() && tcp.ackFlag()) {
            return PacketType::SYN_ACK;
        }

        if (tcp.syn()) {
            return PacketType::SYN;
        }

        if (tcp.fin()) {
            return PacketType::FIN;
        }

        if (tcp.psh()) {
            return PacketType::DATA;
        }

        if (tcp.ackFlag() && tcp.payload.empty()) {
            return PacketType::ACK;
        }

        return PacketType::DATA;
    }

    struct Packet {
        int source = 0;
        int destination = 0;
        int current_node = 0;

        double creation_time = 0.0;
        double departure_time = 0.0;

        int packet_size_bytes = 0;
        int hop_count = 0;

        std::uint64_t session_id = 0;

        TCPSegment tcp;

        PacketType packet_type = PacketType::DATA;

        Packet() = default;

        Packet(
            int source,
            int destination,
            int current_node,
            double creation_time,
            int packet_size_bytes,
            std::uint64_t session_id
        )
            : source(source),
              destination(destination),
              current_node(current_node),
              creation_time(creation_time),
              packet_size_bytes(packet_size_bytes),
              session_id(session_id)
        {
        }
    };

}