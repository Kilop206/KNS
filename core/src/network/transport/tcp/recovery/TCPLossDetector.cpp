#include "network/transport/tcp/recovery/TCPLossDetector.hpp"

namespace kns {

    bool TCPLossDetector::observeAck(
        std::uint32_t ack_number
    ) noexcept
    {
        if (!has_ack_) {
            last_ack_ = ack_number;
            duplicate_ack_count_ = 0;
            has_ack_ = true;
            return false;
        }

        if (ack_number == last_ack_) {
            ++duplicate_ack_count_;
            return true;
        }

        /*
         * A new ACK advances the observation point and
         * invalidates the duplicate-ACK streak.
         */
        last_ack_ = ack_number;
        duplicate_ack_count_ = 0;

        return false;
    }

    bool TCPLossDetector::shouldFastRetransmit() const noexcept
    {
        return duplicate_ack_count_
            >= DUPLICATE_ACK_THRESHOLD;
    }

    std::uint32_t TCPLossDetector::getLastAck() const noexcept
    {
        return last_ack_;
    }

    std::uint32_t TCPLossDetector::getDuplicateAckCount() const noexcept
    {
        return duplicate_ack_count_;
    }

    bool TCPLossDetector::hasObservedAck() const noexcept
    {
        return has_ack_;
    }

    void TCPLossDetector::reset() noexcept
    {
        last_ack_ = 0;
        duplicate_ack_count_ = 0;
        has_ack_ = false;
    }

}