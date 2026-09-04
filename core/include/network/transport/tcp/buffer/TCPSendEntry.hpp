#pragma once

#include <cstddef>
#include <cstdint>

#include "network/transport/tcp/TCPSegment.hpp"

namespace kns {

    struct TCPSendEntry {
        TCPSegment segment;

        // Timestamp da transmissão mais recente.
        double sent_at = 0.0;

        // Indica que este segmento já foi retransmitido.
        // Pelo algoritmo de Karn, uma entrada marcada assim
        // não pode fornecer amostra válida de RTT.
        bool retransmitted = false;

        std::uint32_t sequence_end() const noexcept {
            return segment.seq +
                   static_cast<std::uint32_t>(segment.payloadSize());
        }

        std::size_t payload_size() const noexcept {
            return segment.payloadSize();
        }

        bool isAcknowledged(
            std::uint32_t ack_number
        ) const noexcept {
            return ack_number >= sequence_end();
        }

        bool canMeasureRtt() const noexcept {
            return !retransmitted;
        }

        void markRetransmitted(
            double retransmission_time
        ) noexcept
        {
            sent_at = retransmission_time;
            retransmitted = true;
        }
    };

} // namespace kns