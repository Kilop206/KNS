#pragma once

#include <cstddef>
#include <cstdint>

#include "network/transport/tcp/TCPSegment.hpp"

namespace kns {

    struct TCPReceiveEntry {
        TCPSegment segment;
        double received_at = 0.0;

        std::uint32_t sequence_end() const noexcept {
            return segment.seq +
                   static_cast<std::uint32_t>(segment.payloadSize());
        }

        std::size_t payload_size() const noexcept {
            return segment.payloadSize();
        }
    };

} // namespace kns