#pragma once
#include <cstdint>
#include <memory>
#include <optional>

namespace kns {
class SimulationEngine;
struct Packet;

// Optional lifecycle sink for explicitly modeled abstract workload packets.
// Shared by hop copies; the implementation must finalize a packet at most once.
class PacketOutcome {
public:
    virtual ~PacketOutcome() = default;
    virtual void delivered(SimulationEngine&, const Packet&) = 0;
    virtual void rejected() noexcept = 0;
};

struct PacketWorkload {
    std::uint32_t flow_id = 0;
    std::uint64_t packet_index = 0;
    std::uint8_t priority = 0; // class metadata; existing link FIFO/RED policies apply
    std::uint32_t ttl_hops = 64;
    std::optional<double> deadline_at_s;
    std::shared_ptr<PacketOutcome> outcome;
    bool response = false;
};
}
