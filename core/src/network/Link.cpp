#include "network/Link.hpp"

#include <algorithm>
#include <limits>

#include "engine/core/Random.hpp"

namespace kns {

    Link::Link(
        int a,
        int b,
        double bandwidth_mbps,
        double delay_ms,
        double loss_prob,
        LinkMode mode
    )
        : a_(a),
        b_(b),
        bandwidth_mbps_(bandwidth_mbps),
        delay_ms_(delay_ms),
        loss_prob_(loss_prob),
        mode_(mode)
    {
    }

    int Link::getA() const noexcept {
        return a_;
    }

    int Link::getB() const noexcept {
        return b_;
    }

    int Link::getOtherNode(int node) const noexcept {
        if (node == a_) return b_;
        if (node == b_) return a_;
        return -1;
    }

    double Link::getBandwidthMbps() const noexcept {
        return bandwidth_mbps_;
    }

    void Link::setBandwidthMbps(double value) noexcept {
        bandwidth_mbps_ = value;
    }

    double Link::getDelayMs() const noexcept {
        return delay_ms_;
    }

    void Link::setDelayMs(double value) noexcept {
        delay_ms_ = value;
    }

    double Link::getLossProb() const noexcept {
        return loss_prob_;
    }

    void Link::setLossProb(double value) noexcept {
        loss_prob_ = value;
    }

    LinkMode Link::getMode() const noexcept {
        return mode_;
    }

    void Link::setMode(LinkMode mode) noexcept {
        mode_ = mode;
    }

    Link::DirectionSlot Link::getDirectionSlot(int from, int to) const noexcept {
        if (from == a_ && to == b_) return DirectionSlot::AB;
        if (from == b_ && to == a_) return DirectionSlot::BA;
        return DirectionSlot::Invalid;
    }

    double Link::getNextAvailableTime(int from, int to, double now) const noexcept {
        const DirectionSlot slot = getDirectionSlot(from, to);

        switch (mode_) {
            case LinkMode::FULL_DUPLEX:
                if (slot == DirectionSlot::AB) return std::max(now, busy_until_ab_);
                if (slot == DirectionSlot::BA) return std::max(now, busy_until_ba_);
                return std::numeric_limits<double>::infinity();

            case LinkMode::HALF_DUPLEX:
                if (slot == DirectionSlot::Invalid) return std::numeric_limits<double>::infinity();
                return std::max(now, busy_until_shared_);

            case LinkMode::SIMPLEX:
                if (slot != DirectionSlot::AB) return std::numeric_limits<double>::infinity();
                return std::max(now, busy_until_ab_);

            default:
                return std::numeric_limits<double>::infinity();
        }
    }

    void Link::reserveTransmission(int from, int to, double busy_until) const noexcept {
        const DirectionSlot slot = getDirectionSlot(from, to);

        switch (mode_) {
            case LinkMode::FULL_DUPLEX:
                if (slot == DirectionSlot::AB) {
                    busy_until_ab_ = std::max(busy_until_ab_, busy_until);
                } else if (slot == DirectionSlot::BA) {
                    busy_until_ba_ = std::max(busy_until_ba_, busy_until);
                }
                break;

            case LinkMode::HALF_DUPLEX:
                busy_until_shared_ = std::max(busy_until_shared_, busy_until);
                break;

            case LinkMode::SIMPLEX:
                if (slot == DirectionSlot::AB) {
                    busy_until_ab_ = std::max(busy_until_ab_, busy_until);
                }
                break;
        }
    }

    bool Link::isBusy(int from, int to, double now) const noexcept {
        return now < getNextAvailableTime(from, to, now);
    }

    bool Link::should_drop() const {
        if (loss_prob_ <= 0.0) {
            return false;
        }

        const double r = Random::uniform01();
        return r < loss_prob_;
    }
    
    std::size_t Link::estimatedQueueSize(double /*now*/, int /*from*/, int /*to*/) const {
        return getQueueSize();
    }

    bool Link::canQueue() const noexcept
    {
        return queued_packets_ < queue_capacity_;
    }

    void Link::enqueuePacket() noexcept
    {
        ++queued_packets_;
    }

    void Link::dequeuePacket() noexcept
    {
        if (queued_packets_ > 0) {
            --queued_packets_;
        }
    }

    std::size_t Link::getQueueSize() const noexcept
    {
        return queued_packets_;
    }

    std::size_t Link::getQueueCapacity() const noexcept
    {
        return queue_capacity_;
    }
}