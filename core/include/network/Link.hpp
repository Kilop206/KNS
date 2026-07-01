#pragma once

#include <cstdint>
#include <limits>

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

    private:
        enum class DirectionSlot {
            AB,
            BA,
            Shared,
            Invalid
        };

        DirectionSlot getDirectionSlot(int from, int to) const noexcept;

        const int a_;
        const int b_;

        double bandwidth_mbps_;
        double delay_ms_;
        double loss_prob_;
        LinkMode mode_;

        mutable double busy_until_ab_ = 0.0;
        mutable double busy_until_ba_ = 0.0;
        mutable double busy_until_shared_ = 0.0;
    };

}