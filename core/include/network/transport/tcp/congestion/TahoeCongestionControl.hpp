#pragma once

#include <cstdint>

#include "network/transport/tcp/congestion/CongestionControl.hpp"

namespace kns
{
    class TahoeCongestionControl final : public CongestionControl
    {
    public:
        static constexpr std::uint32_t DEFAULT_SSTHRESH = 65535;

        explicit TahoeCongestionControl(
            std::uint32_t mss,
            std::uint32_t initial_ssthresh = DEFAULT_SSTHRESH
        );

        std::uint32_t getCwnd() const noexcept override;
        std::uint32_t getSsthresh() const noexcept override;
        std::uint32_t getMss() const noexcept override;

        bool canSend(
            std::uint32_t bytes_in_flight,
            std::uint32_t segment_size
        ) const noexcept override;

        void onAck(
            std::uint32_t acknowledged_bytes
        ) noexcept override;

        void onLoss() noexcept override;

        void reset() noexcept override;

    private:
        std::uint32_t mss_;
        std::uint32_t initial_ssthresh_;

        std::uint32_t cwnd_;
        std::uint32_t ssthresh_;
    };
}