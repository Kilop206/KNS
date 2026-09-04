#include "network/transport/tcp/buffer/TCPSendBuffer.hpp"

#include <cmath>
#include <limits>
#include <utility>

namespace kns {

    bool TCPSendBuffer::push(
        TCPSendEntry entry
    )
    {
        if (
            capacity_ != 0 &&
            entries_.size() >= capacity_
        ) {
            return false;
        }

        entries_.push_back(
            std::move(entry)
        );

        return true;
    }

    std::size_t TCPSendBuffer::acknowledge(
        std::uint32_t ack_number
    )
    {
        std::size_t removed = 0;

        while (
            !entries_.empty() &&
            entries_.front().isAcknowledged(
                ack_number
            )
        ) {
            entries_.pop_front();
            ++removed;
        }

        return removed;
    }

    std::optional<TCPSendEntry>
    TCPSendBuffer::popFront()
    {
        if (entries_.empty()) {
            return std::nullopt;
        }

        TCPSendEntry entry =
            std::move(entries_.front());

        entries_.pop_front();

        return entry;
    }

    bool TCPSendBuffer::markRetransmitted(
        std::uint32_t sequence,
        double retransmission_time
    ) noexcept
    {
        for (auto& entry : entries_) {
            if (entry.segment.seq == sequence) {
                entry.markRetransmitted(
                    retransmission_time
                );

                return true;
            }
        }

        return false;
    }

    std::optional<double>
    TCPSendBuffer::acknowledgeAndGetRtt(
        std::uint32_t ack_number,
        double acknowledgement_time
    )
    {
        if (entries_.empty()) {
            return std::nullopt;
        }

        std::optional<double> rtt_sample;

        while (
            !entries_.empty() &&
            entries_.front().isAcknowledged(
                ack_number
            )
        ) {
            const TCPSendEntry& entry =
                entries_.front();

            if (
                !rtt_sample.has_value() &&
                entry.canMeasureRtt()
            ) {
                const double sample =
                    acknowledgement_time -
                    entry.sent_at;

                if (
                    std::isfinite(sample) &&
                    sample > 0.0
                ) {
                    rtt_sample = sample;
                }
            }

            entries_.pop_front();
        }

        return rtt_sample;
    }

} // namespace kns