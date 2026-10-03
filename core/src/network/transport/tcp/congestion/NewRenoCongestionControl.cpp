#include "network/transport/tcp/congestion/NewRenoCongestionControl.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace kns
{

    NewRenoCongestionControl::NewRenoCongestionControl(
        std::uint32_t mss,
        std::uint32_t initial_ssthresh
    )
        : mss_(mss),
        initial_ssthresh_(initial_ssthresh),
        cwnd_(mss),
        ssthresh_(std::max(
            initial_ssthresh,
            std::uint32_t{2} * mss
        )),
        recovery_ssthresh_(0),
        fast_recovery_(false),
        partial_ack_observed_(false)
    {
        if (mss_ == 0) {
            throw std::invalid_argument(
                "NewRenoCongestionControl requires a non-zero MSS"
            );
        }
    }

    std::uint32_t NewRenoCongestionControl::getCwnd() const noexcept
    {
        return cwnd_;
    }

    std::uint32_t NewRenoCongestionControl::getSsthresh() const noexcept
    {
        return ssthresh_;
    }

    std::uint32_t NewRenoCongestionControl::getMss() const noexcept
    {
        return mss_;
    }

    bool NewRenoCongestionControl::canSend(
        std::uint32_t bytes_in_flight,
        std::uint32_t segment_size
    ) const noexcept
    {
        if (bytes_in_flight > cwnd_) {
            return false;
        }

        return segment_size <= (cwnd_ - bytes_in_flight);
    }

    void NewRenoCongestionControl::onAck(
        std::uint32_t acknowledged_bytes
    ) noexcept
    {
        if (acknowledged_bytes == 0) {
            return;
        }

        if (fast_recovery_) {
            return;
        }

        if (cwnd_ < ssthresh_) {
            const std::uint32_t increase =
                std::min(acknowledged_bytes, mss_);

            const std::uint32_t max_value =
                std::numeric_limits<std::uint32_t>::max();

            if (cwnd_ > max_value - increase) {
                cwnd_ = max_value;
            } else {
                cwnd_ += increase;
            }

            return;
        }

        const std::uint64_t numerator =
            static_cast<std::uint64_t>(mss_) *
            static_cast<std::uint64_t>(mss_);

        const std::uint64_t increase =
            std::max<std::uint64_t>(
                1,
                numerator / cwnd_
            );

        const std::uint32_t max_value =
            std::numeric_limits<std::uint32_t>::max();

        if (increase >= max_value - cwnd_) {
            cwnd_ = max_value;
        } else {
            cwnd_ += static_cast<std::uint32_t>(increase);
        }
    }

    void NewRenoCongestionControl::onLoss() noexcept
    {
        const std::uint32_t minimum_ssthresh =
            std::uint32_t{2} * mss_;

        if (fast_recovery_) {
            ssthresh_ = std::max(
                minimum_ssthresh,
                recovery_ssthresh_
            );
        } else {
            ssthresh_ = std::max(
                minimum_ssthresh,
                cwnd_ / 2
            );
        }

        cwnd_ = mss_;
        fast_recovery_ = false;
        partial_ack_observed_ = false;
    }

    void NewRenoCongestionControl::onFastRetransmit(
        std::uint32_t flight_size
    ) noexcept
    {
        const std::uint32_t minimum_ssthresh =
            std::uint32_t{2} * mss_;

        ssthresh_ = std::max(
            minimum_ssthresh,
            flight_size / 2
        );

        recovery_ssthresh_ = ssthresh_;

        const std::uint32_t recovery_increase =
            std::uint32_t{3} * mss_;

        const std::uint32_t max_value =
            std::numeric_limits<std::uint32_t>::max();

        if (ssthresh_ > max_value - recovery_increase) {
            cwnd_ = max_value;
        } else {
            cwnd_ = ssthresh_ + recovery_increase;
        }

        fast_recovery_ = true;
        partial_ack_observed_ = false;
    }

    void NewRenoCongestionControl::onDuplicateAck() noexcept
    {
        if (!fast_recovery_) {
            return;
        }

        const std::uint32_t max_value =
            std::numeric_limits<std::uint32_t>::max();

        if (cwnd_ > max_value - mss_) {
            cwnd_ = max_value;
        } else {
            cwnd_ += mss_;
        }
    }

    void NewRenoCongestionControl::onRecoveryAck(
        std::uint32_t acknowledged_bytes,
        bool recovery_complete
    ) noexcept
    {
        if (!fast_recovery_) {
            return;
        }

        if (recovery_complete) {
            cwnd_ = ssthresh_;
            fast_recovery_ = false;
            partial_ack_observed_ = false;
            return;
        }

        if (acknowledged_bytes == 0) {
            return;
        }

        /*
         * Partial ACKs keep NewReno in fast recovery.
         *
         * Deflate cwnd by the newly acknowledged data, then add
         * one MSS back when at least one MSS of new data was
         * acknowledged. This keeps the sender near ssthresh
         * while another outstanding segment is recovered.
         */
        if (acknowledged_bytes >= cwnd_) {
            cwnd_ = mss_;
        } else {
            cwnd_ -= acknowledged_bytes;
        }

        if (acknowledged_bytes >= mss_) {
            const std::uint32_t max_value =
                std::numeric_limits<std::uint32_t>::max();

            if (cwnd_ > max_value - mss_) {
                cwnd_ = max_value;
            } else {
                cwnd_ += mss_;
            }
        }

        const std::uint32_t minimum_window =
            mss_;

        if (cwnd_ < minimum_window) {
            cwnd_ = minimum_window;
        }

        partial_ack_observed_ = true;
    }

    void NewRenoCongestionControl::reset() noexcept
    {
        cwnd_ = mss_;

        ssthresh_ = std::max(
            initial_ssthresh_,
            std::uint32_t{2} * mss_
        );

        fast_recovery_ = false;
        partial_ack_observed_ = false;
        recovery_ssthresh_ = 0;
    }

    bool NewRenoCongestionControl::inFastRecovery() const noexcept
    {
        return fast_recovery_;
    }

    bool NewRenoCongestionControl::partialAckObserved() const noexcept
    {
        return partial_ack_observed_;
    }
}