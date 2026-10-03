#pragma once

#include <cstdint>

#include "network/transport/tcp/congestion/CongestionControl.hpp"

namespace kns
{
    class CubicCongestionControl final : public CongestionControl
    {
    public:
        static constexpr std::uint32_t DEFAULT_SSTHRESH = 65535;

        static constexpr double DEFAULT_BETA = 0.7;
        static constexpr double DEFAULT_C = 0.4;

        explicit CubicCongestionControl(
            std::uint32_t mss,
            std::uint32_t initial_ssthresh = DEFAULT_SSTHRESH,
            double beta = DEFAULT_BETA,
            double c = DEFAULT_C
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

        void onFastRetransmit(
            std::uint32_t flight_size
        ) noexcept override;

        void onDuplicateAck() noexcept override;

        void onRecoveryAck(
            std::uint32_t acknowledged_bytes,
            bool recovery_complete = true
        ) noexcept override;

        void reset() noexcept override;

        double getBeta() const noexcept;
        double getC() const noexcept;

        std::uint32_t getWMax() const noexcept;

        bool inFastRecovery() const noexcept;

    private:
        std::uint32_t mss_;
        std::uint32_t initial_ssthresh_;

        std::uint32_t cwnd_;
        std::uint32_t ssthresh_;

        std::uint32_t w_max_;

        double beta_;
        double c_;

        bool fast_recovery_;
    };
}