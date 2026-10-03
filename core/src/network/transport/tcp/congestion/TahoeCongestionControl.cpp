#include "network/transport/tcp/congestion/TahoeCongestionControl.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace kns
{
    TahoeCongestionControl::TahoeCongestionControl(
        std::uint32_t mss,
        std::uint32_t initial_ssthresh
    )
        : mss_(mss),
          initial_ssthresh_(initial_ssthresh),
          cwnd_(mss),
          ssthresh_(std::max(
              initial_ssthresh,
              std::uint32_t{2} * mss
          ))
    {
        if (mss_ == 0) {
            throw std::invalid_argument(
                "TahoeCongestionControl requires a non-zero MSS"
            );
        }
    }

    std::uint32_t TahoeCongestionControl::getCwnd() const noexcept
    {
        return cwnd_;
    }

    std::uint32_t TahoeCongestionControl::getSsthresh() const noexcept
    {
        return ssthresh_;
    }

    std::uint32_t TahoeCongestionControl::getMss() const noexcept
    {
        return mss_;
    }

    bool TahoeCongestionControl::canSend(
        std::uint32_t bytes_in_flight,
        std::uint32_t segment_size
    ) const noexcept
    {
        if (bytes_in_flight > cwnd_) {
            return false;
        }

        return segment_size <= (cwnd_ - bytes_in_flight);
    }

    void TahoeCongestionControl::onAck(
        std::uint32_t acknowledged_bytes
    ) noexcept
    {
        if (acknowledged_bytes == 0) {
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

    void TahoeCongestionControl::onLoss() noexcept
    {
        const std::uint32_t minimum_ssthresh =
            std::uint32_t{2} * mss_;

        const std::uint32_t reduced_ssthresh =
            cwnd_ / 2;

        ssthresh_ = std::max(
            minimum_ssthresh,
            reduced_ssthresh
        );

        cwnd_ = mss_;
    }

    void TahoeCongestionControl::reset() noexcept
    {
        cwnd_ = mss_;
        ssthresh_ = std::max(
            initial_ssthresh_,
            std::uint32_t{2} * mss_
        );
    }
}