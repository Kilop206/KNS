#pragma once

#include <cstddef>
#include <cstdint>

#include "network/transport/tcp/TCPSegment.hpp"

namespace kns {

    struct TCPSendEntry {
        TCPSegment segment;
        double sent_at = 0.0;

        std::uint32_t sequence_end() const noexcept {
            return segment.seq +
                   static_cast<std::uint32_t>(segment.payloadSize());
        }

        std::size_t payload_size() const noexcept {
            return segment.payloadSize();
        }

        bool isAcknowledged(std::uint32_t ack_number) const noexcept {
            return ack_number >= sequence_end();
        }
    };

} // namespace kns