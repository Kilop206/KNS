#pragma once

#include <cstdint>
#include <limits>
#include <deque>
#include <cstddef>

#include "enums/LinkMode.hpp"

namespace kns {

class Link {
    public:
        Link(
            int a,
            int b,
            double bandwidth_mbps,
            double delay_ms,
            double loss_prob = 0.0,
            LinkMode mode = LinkMode::FULL_DUPLEX
        );

        /// Stable numeric identity assigned at construction. Unique within a simulation run.
        std::uint64_t getId() const noexcept;

        int getA() const noexcept;
        int getB() const noexcept;
        int getOtherNode(int node) const noexcept;

        double getBandwidthMbps() const noexcept;
        void setBandwidthMbps(double value) noexcept;

        double getDelayMs() const noexcept;
        void setDelayMs(double value) noexcept;

        double getLossProb() const noexcept;
        void setLossProb(double value) noexcept;

        LinkMode getMode() const noexcept;
        void setMode(LinkMode mode) noexcept;

        bool isBusy(int from, int to, double now) const noexcept;

        double getNextAvailableTime(int from, int to, double now) const noexcept;
        void reserveTransmission(int from, int to, double busy_until) const noexcept;

        bool should_drop() const;

        std::size_t estimatedQueueSize(double now, int from, int to) const;

        bool canQueue() const noexcept;

        /// Record a transmission in the FIFO queue. Does NOT check capacity —
        /// caller must call canQueue() first. Supersedes the bare enqueuePacket().
        void enqueueTransmission(int from, int to,
                                  double departure_time,
                                  double arrival_time) noexcept;

        /// Remove the oldest matching transmission from the FIFO queue.
        /// Returns true if a matching entry was found and removed.
        bool dequeueTransmission(int from, int to,
                                  double departure_time,
                                  double arrival_time) noexcept;

        /// Legacy counter-only helpers kept for callers that have not yet been
        /// migrated to the typed FIFO methods.
        void enqueuePacket() noexcept;
        void dequeuePacket() noexcept;

        std::size_t getQueueSize() const noexcept;
        std::size_t getQueueCapacity() const noexcept;

        // Up/down state for GUI toggling
        bool isUp() const noexcept;
        void setUp(bool up) noexcept;

    private:
        
        enum class DirectionSlot {
            AB,
            BA,
            Shared,
            Invalid
        };

        struct LinkTransmission {
            int from = -1;
            int to = -1;
            double departure_time = 0.0;
            double arrival_time = 0.0;
        };

        double busy_until_ = 0.0;
        std::deque<LinkTransmission> queue_;

        DirectionSlot getDirectionSlot(int from, int to) const noexcept;

        const std::uint64_t id_;
        static std::uint64_t next_id_;

        const int a_;
        const int b_;

        double bandwidth_mbps_;
        double delay_ms_;
        double loss_prob_;
        LinkMode mode_;

        mutable double busy_until_ab_ = 0.0;
        mutable double busy_until_ba_ = 0.0;
        mutable double busy_until_shared_ = 0.0;

        std::size_t queue_capacity_ = 32;
        std::size_t queued_packets_ = 0;

        bool up_ = true;
    };
}
