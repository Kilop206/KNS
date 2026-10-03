#pragma once

<<<<<<< HEAD
#include "engine/core/SimulationEngine.hpp"
#include "network/Packet.hpp"

namespace kns {
    bool sendPacketThroughTopology(SimulationEngine& engine, Packet pkt);
}
=======
#include <cstdint>

namespace kns {

    class SimulationEngine;
    struct Packet;

    class PacketUtils {
    public:
        static bool sendPacketThroughTopology(
            SimulationEngine& engine,
            const Packet& pkt
        );

        static bool releasePacketThroughTopology(
            SimulationEngine& engine,
            const Packet& pkt
        );

        /// Send a TCP RST from `from` to `to`, acknowledging `remote_seq`.
        /// The caller provides the packet correlation ID to preserve.
        static bool sendReset(
            SimulationEngine& engine,
            int from,
            int to,
            std::uint32_t remote_seq,
            std::uint64_t correlation_id,
            std::uint16_t source_port = 0,
            std::uint16_t destination_port = 0
        );
    };
}
>>>>>>> 879e9a30eb706359e007b3218a4c881c257cd5bc
