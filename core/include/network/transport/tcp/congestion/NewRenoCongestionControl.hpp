#pragma once

#include <cstdint>

#include "network/transport/tcp/congestion/CongestionControl.hpp"

namespace kns
{
    class NewRenoCongestionControl final : public CongestionControl
    {
    public:
        static constexpr std::uint32_t DEFAULT_SSTHRESH = 65535;

        explicit NewRenoCongestionControl(
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

        void onFastRetransmit(
            std::uint32_t flight_size
        ) noexcept override;

        void onDuplicateAck() noexcept override;

        void onRecoveryAck(
            std::uint32_t acknowledged_bytes,
            bool recovery_complete
        ) noexcept override;

        void reset() noexcept override;

        bool inFastRecovery() const noexcept;

        bool partialAckObserved() const noexcept;

    private:
        std::uint32_t mss_;
        std::uint32_t initial_ssthresh_;

        std::uint32_t cwnd_;
        std::uint32_t ssthresh_;

        std::uint32_t recovery_ssthresh_;

        bool fast_recovery_;
        bool partial_ack_observed_;
    };
}