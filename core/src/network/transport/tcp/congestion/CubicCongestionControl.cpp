#include "network/transport/tcp/congestion/CubicCongestionControl.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kns
{
    CubicCongestionControl::CubicCongestionControl(
        std::uint32_t mss,
        std::uint32_t initial_ssthresh,
        double beta,
        double c
    )
        : mss_(mss),
          initial_ssthresh_(initial_ssthresh),
          cwnd_(mss),
          ssthresh_(std::max(
              initial_ssthresh,
              std::uint32_t{2} * mss
          )),
          w_max_(mss),
          beta_(beta),
          c_(c),
          fast_recovery_(false)
    {
        if (mss_ == 0) {
            throw std::invalid_argument(
                "CubicCongestionControl requires a non-zero MSS"
            );
        }

        if (!std::isfinite(beta_) || beta_ <= 0.0 || beta_ >= 1.0) {
            throw std::invalid_argument(
                "CubicCongestionControl beta must be between 0 and 1"
            );
        }

        if (!std::isfinite(c_) || c_ <= 0.0) {
            throw std::invalid_argument(
                "CubicCongestionControl C must be positive"
            );
        }
    }

    std::uint32_t CubicCongestionControl::getCwnd() const noexcept
    {
        return cwnd_;
    }

    std::uint32_t CubicCongestionControl::getSsthresh() const noexcept
    {
        return ssthresh_;
    }

    std::uint32_t CubicCongestionControl::getMss() const noexcept
    {
        return mss_;
    }

    bool CubicCongestionControl::canSend(
        std::uint32_t bytes_in_flight,
        std::uint32_t segment_size
    ) const noexcept
    {
        if (bytes_in_flight > cwnd_) {
            return false;
        }

        return segment_size <= (cwnd_ - bytes_in_flight);
    }

    void CubicCongestionControl::onAck(
        std::uint32_t acknowledged_bytes
    ) noexcept
    {
        if (acknowledged_bytes == 0 || fast_recovery_) {
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

        /*
         * The full CUBIC specification expresses the window growth
         * as a function of elapsed time and RTT. Until the simulator
         * exposes those values directly to this interface, use the
         * acknowledged-byte count as a deterministic progress unit.
         *
         * This preserves the cubic shape while keeping the algorithm
         * independent from wall-clock timing.
         */
        const double normalized_ack =
            static_cast<double>(acknowledged_bytes) /
            static_cast<double>(mss_);

        const double current_window =
            static_cast<double>(cwnd_) /
            static_cast<double>(mss_);

        const double max_window =
            static_cast<double>(w_max_) /
            static_cast<double>(mss_);

        const double distance =
            current_window - max_window;

        const double cubic_target =
            static_cast<double>(w_max_) +
            c_ * std::pow(
                normalized_ack - std::cbrt(
                    max_window * (1.0 - beta_) / c_
                ),
                3.0
            ) * static_cast<double>(mss_);

        const double target =
            std::max(
                static_cast<double>(mss_),
                cubic_target
            );

        const double increase =
            std::max(
                1.0,
                (target - static_cast<double>(cwnd_)) /
                std::max(1.0, current_window)
            );

        static_cast<void>(distance);

        const std::uint32_t max_value =
            std::numeric_limits<std::uint32_t>::max();

        const auto increase_bytes =
            static_cast<std::uint64_t>(
                std::ceil(increase)
            );

        if (increase_bytes >=
            static_cast<std::uint64_t>(max_value - cwnd_)) {
            cwnd_ = max_value;
        } else {
            cwnd_ += static_cast<std::uint32_t>(increase_bytes);
        }
    }

    void CubicCongestionControl::onLoss() noexcept
    {
        const std::uint32_t minimum_ssthresh =
            std::uint32_t{2} * mss_;

        w_max_ = cwnd_;

        const auto reduced =
            static_cast<std::uint64_t>(
                std::floor(
                    static_cast<double>(cwnd_) * beta_
                )
            );

        const auto reduced_window =
            std::max<std::uint64_t>(
                mss_,
                reduced
            );

        ssthresh_ = std::max(
            minimum_ssthresh,
            static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    reduced_window,
                    std::numeric_limits<std::uint32_t>::max()
                )
            )
        );

        cwnd_ = ssthresh_;
        fast_recovery_ = false;
    }

    void CubicCongestionControl::onFastRetransmit(
        std::uint32_t flight_size
    ) noexcept
    {
        const std::uint32_t minimum_ssthresh =
            std::uint32_t{2} * mss_;

        w_max_ = cwnd_;

        const auto reduced =
            static_cast<std::uint64_t>(
                std::floor(
                    static_cast<double>(flight_size) * beta_
                )
            );

        const auto reduced_window =
            std::max<std::uint64_t>(
                mss_,
                reduced
            );

        ssthresh_ = std::max(
            minimum_ssthresh,
            static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    reduced_window,
                    std::numeric_limits<std::uint32_t>::max()
                )
            )
        );

        cwnd_ = ssthresh_;
        fast_recovery_ = true;
    }

    void CubicCongestionControl::onDuplicateAck() noexcept
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

    void CubicCongestionControl::onRecoveryAck(
        std::uint32_t acknowledged_bytes,
        bool recovery_complete
    ) noexcept
    {
        static_cast<void>(acknowledged_bytes);

        if (!fast_recovery_) {
            return;
        }

        if (!recovery_complete) {
            return;
        }

        cwnd_ = ssthresh_;
        fast_recovery_ = false;
    }

    void CubicCongestionControl::reset() noexcept
    {
        cwnd_ = mss_;

        ssthresh_ = std::max(
            initial_ssthresh_,
            std::uint32_t{2} * mss_
        );

        w_max_ = mss_;
        fast_recovery_ = false;
    }

    double CubicCongestionControl::getBeta() const noexcept
    {
        return beta_;
    }

    double CubicCongestionControl::getC() const noexcept
    {
        return c_;
    }

    std::uint32_t CubicCongestionControl::getWMax() const noexcept
    {
        return w_max_;
    }

    bool CubicCongestionControl::inFastRecovery() const noexcept
    {
        return fast_recovery_;
    }
}