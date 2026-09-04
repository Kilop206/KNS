#pragma once

#include <cstdint>

namespace kns {

    class TCPLossDetector {
    public:
        static constexpr std::uint32_t DUPLICATE_ACK_THRESHOLD = 3;

        TCPLossDetector() noexcept = default;

        /**
         * Observe an incoming ACK number.
         *
         * Returns true when the ACK is a duplicate of the
         * currently tracked ACK number.
         */
        bool observeAck(std::uint32_t ack_number) noexcept;

        /**
         * Returns true when at least three duplicate ACKs
         * have been observed for the same ACK number.
         */
        bool shouldFastRetransmit() const noexcept;

        std::uint32_t getLastAck() const noexcept;

        std::uint32_t getDuplicateAckCount() const noexcept;

        bool hasObservedAck() const noexcept;

        void reset() noexcept;

    private:
        std::uint32_t last_ack_ = 0;
        std::uint32_t duplicate_ack_count_ = 0;
        bool has_ack_ = false;
    };

}