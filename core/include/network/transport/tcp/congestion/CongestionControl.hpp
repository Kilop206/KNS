#pragma once

#include <cstdint>

namespace kns
{
    /**
     * @brief Common interface for TCP congestion control algorithms.
     *
     * Congestion state is maintained in bytes.
     *
     * - cwnd: current congestion window;
     * - ssthresh: threshold between slow start and congestion avoidance;
     * - mss: maximum segment size used by the algorithm.
     *
     * Concrete implementations (Tahoe, Reno, NewReno and CUBIC)
     * define how this state evolves over time.
     */
    class CongestionControl
    {
    public:
        virtual ~CongestionControl() = default;

        /**
         * @brief Returns the current congestion window.
         */
        virtual std::uint32_t getCwnd() const noexcept = 0;

        /**
         * @brief Returns the current slow-start threshold.
         */
        virtual std::uint32_t getSsthresh() const noexcept = 0;

        /**
         * @brief Returns the maximum segment size used by the
         * congestion control algorithm.
         */
        virtual std::uint32_t getMss() const noexcept = 0;

        /**
         * @brief Checks whether a new segment may be transmitted.
         *
         * @param bytes_in_flight Number of bytes currently unacknowledged.
         * @param segment_size Size of the new segment in bytes.
         *
         * @return true if the new segment fits within the current cwnd.
         */
        virtual bool canSend(
            std::uint32_t bytes_in_flight,
            std::uint32_t segment_size
        ) const noexcept = 0;

        /**
         * @brief Notifies the algorithm that new bytes were acknowledged.
         */
        virtual void onAck(
            std::uint32_t acknowledged_bytes
        ) noexcept = 0;

        /**
         * @brief Notifies the algorithm that packet loss was detected
         * by a retransmission timeout.
         */
        virtual void onLoss() noexcept = 0;

        /**
         * @brief Notifies the algorithm that fast retransmit was triggered.
         *
         * The default implementation treats fast retransmit as a
         * generic loss event.
         *
         * @param flight_size Current amount of outstanding data.
         */
        virtual void onFastRetransmit(
            std::uint32_t flight_size
        ) noexcept
        {
            static_cast<void>(flight_size);
            onLoss();
        }

        /**
         * @brief Notifies the algorithm that another duplicate ACK
         * arrived while recovering from fast retransmit.
         */
        virtual void onDuplicateAck() noexcept
        {
        }

        /**
         * @brief Notifies the algorithm that an ACK was received
         * while in fast recovery.
         *
         * @param acknowledged_bytes Number of newly acknowledged bytes.
         * @param recovery_complete true when the ACK covers all data
         *                           outstanding when fast recovery began.
         */
        virtual void onRecoveryAck(
            std::uint32_t acknowledged_bytes,
            bool recovery_complete = true
        ) noexcept
        {
            static_cast<void>(acknowledged_bytes);
            static_cast<void>(recovery_complete);
        }

        /**
         * @brief Restores the algorithm to its initial state.
         */
        virtual void reset() noexcept = 0;
    };
}