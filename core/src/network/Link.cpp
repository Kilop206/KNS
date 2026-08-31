#include "network/Link.hpp"

#include <algorithm>
#include <limits>

#include "engine/core/Random.hpp"

namespace kns {

    std::uint64_t Link::next_id_ = 0;

    Link::Link(
        int a,
        int b,
        double bandwidth_mbps,
        double delay_ms,
        double loss_prob,
        LinkMode mode
    )
        : id_(next_id_++),
        a_(a),
        b_(b),
        bandwidth_mbps_(bandwidth_mbps),
        delay_ms_(delay_ms),
        loss_prob_(loss_prob),
        mode_(mode),
        up_(true)
    {
    }

    std::uint64_t Link::getId() const noexcept
    {
        return id_;
    }

    int Link::getA() const noexcept
    {
        return a_;
    }

    int Link::getB() const noexcept
    {
        return b_;
    }

    int Link::getOtherNode(int node) const noexcept
    {
        if (node == a_) {
            return b_;
        }

        if (node == b_) {
            return a_;
        }

        return -1;
    }

    double Link::getBandwidthMbps() const noexcept
    {
        return bandwidth_mbps_;
    }

    void Link::setBandwidthMbps(double value) noexcept
    {
        bandwidth_mbps_ = value;
    }

    double Link::getDelayMs() const noexcept
    {
        return delay_ms_;
    }

    void Link::setDelayMs(double value) noexcept
    {
        delay_ms_ = value;
    }

    double Link::getLossProb() const noexcept
    {
        return loss_prob_;
    }

    void Link::setLossProb(double value) noexcept
    {
        loss_prob_ = value;
    }

    LinkMode Link::getMode() const noexcept
    {
        return mode_;
    }

    void Link::setMode(LinkMode mode) noexcept
    {
        mode_ = mode;
    }

    Link::DirectionSlot Link::getDirectionSlot(
        int from,
        int to
    ) const noexcept
    {
        if (from == a_ && to == b_) {
            return DirectionSlot::AB;
        }

        if (from == b_ && to == a_) {
            return DirectionSlot::BA;
        }

        return DirectionSlot::Invalid;
    }

    double Link::getNextAvailableTime(
        int from,
        int to,
        double now
    ) const noexcept
    {
        const DirectionSlot slot = getDirectionSlot(from, to);

        switch (mode_) {
            case LinkMode::FULL_DUPLEX:
                if (slot == DirectionSlot::AB) {
                    return std::max(now, busy_until_ab_);
                }

                if (slot == DirectionSlot::BA) {
                    return std::max(now, busy_until_ba_);
                }

                return std::numeric_limits<double>::infinity();

            case LinkMode::HALF_DUPLEX:
                if (slot == DirectionSlot::Invalid) {
                    return std::numeric_limits<double>::infinity();
                }

                return std::max(now, busy_until_shared_);

            case LinkMode::SIMPLEX:
                if (slot != DirectionSlot::AB) {
                    return std::numeric_limits<double>::infinity();
                }

                return std::max(now, busy_until_ab_);

            default:
                return std::numeric_limits<double>::infinity();
        }
    }

    void Link::reserveTransmission(
        int from,
        int to,
        double busy_until
    ) const noexcept
    {
        const DirectionSlot slot = getDirectionSlot(from, to);

        switch (mode_) {
            case LinkMode::FULL_DUPLEX:
                if (slot == DirectionSlot::AB) {
                    busy_until_ab_ =
                        std::max(busy_until_ab_, busy_until);
                } else if (slot == DirectionSlot::BA) {
                    busy_until_ba_ =
                        std::max(busy_until_ba_, busy_until);
                }
                break;

            case LinkMode::HALF_DUPLEX:
                if (slot != DirectionSlot::Invalid) {
                    busy_until_shared_ =
                        std::max(busy_until_shared_, busy_until);
                }
                break;

            case LinkMode::SIMPLEX:
                if (slot == DirectionSlot::AB) {
                    busy_until_ab_ =
                        std::max(busy_until_ab_, busy_until);
                }
                break;

            default:
                break;
        }
    }

    bool Link::isBusy(
        int from,
        int to,
        double now
    ) const noexcept
    {
        return now < getNextAvailableTime(from, to, now);
    }

    bool Link::should_drop() const
    {
        if (loss_prob_ <= 0.0) {
            return false;
        }

        const double r = Random::uniform01();
        return r < loss_prob_;
    }

    std::size_t Link::estimatedQueueSize(
        double /*now*/,
        int from,
        int to
    ) const
    {
        const DirectionSlot slot = getQueueSlot(from, to);

        switch (mode_) {
            case LinkMode::FULL_DUPLEX:
                if (slot == DirectionSlot::AB) {
                    return queue_ab_.size();
                }

                if (slot == DirectionSlot::BA) {
                    return queue_ba_.size();
                }

                return 0;

            case LinkMode::HALF_DUPLEX:
                if (slot == DirectionSlot::Invalid) {
                    return 0;
                }

                return queue_shared_.size();

            case LinkMode::SIMPLEX:
                if (slot != DirectionSlot::AB) {
                    return 0;
                }

                return queue_ab_.size();

            default:
                return 0;
        }
    }

    bool Link::canQueue(int from, int to) const noexcept
    {
        if (!up_) {
            return false;
        }

        const DirectionSlot slot = getQueueSlot(from, to);

        switch (mode_) {
            case LinkMode::FULL_DUPLEX:
                if (slot == DirectionSlot::AB) {
                    return queue_ab_.size() < queue_capacity_;
                }

                if (slot == DirectionSlot::BA) {
                    return queue_ba_.size() < queue_capacity_;
                }

                return false;

            case LinkMode::HALF_DUPLEX:
                if (slot == DirectionSlot::Invalid) {
                    return false;
                }

                return queue_shared_.size() < queue_capacity_;

            case LinkMode::SIMPLEX:
                if (slot != DirectionSlot::AB) {
                    return false;
                }

                return queue_ab_.size() < queue_capacity_;

            default:
                return false;
        }
    }

    void Link::enqueueTransmission(
        int from,
        int to,
        double departure_time,
        double arrival_time
    ) noexcept
    {
        const DirectionSlot slot = getQueueSlot(from, to);

        if (!canQueue(from, to)) {
            return;
        }

        queueForSlot(slot).push_back({
            from,
            to,
            departure_time,
            arrival_time
        });
    }

    bool Link::dequeueTransmission(
        int from,
        int to,
        double departure_time,
        double arrival_time
    ) noexcept
    {
        const DirectionSlot slot = getQueueSlot(from, to);

        if (slot == DirectionSlot::Invalid) {
            return false;
        }

        auto& queue = queueForSlot(slot);

        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (
                it->from == from &&
                it->to == to &&
                it->departure_time == departure_time &&
                it->arrival_time == arrival_time
            ) {
                queue.erase(it);
                return true;
            }
        }

        return false;
    }

    std::size_t Link::getQueueSize() const noexcept
    {
        return queue_ab_.size()
            + queue_ba_.size()
            + queue_shared_.size();
    }

    std::size_t Link::getQueueCapacity() const noexcept
    {
        return queue_capacity_;
    }

    bool Link::isUp() const noexcept
    {
        return up_;
    }

    void Link::setUp(bool up) noexcept
    {
        up_ = up;
    }

    std::deque<Link::LinkTransmission>&
    Link::queueForSlot(DirectionSlot slot) noexcept
    {
        switch (slot) {
            case DirectionSlot::AB:
                return queue_ab_;

            case DirectionSlot::BA:
                return queue_ba_;

            case DirectionSlot::Shared:
                return queue_shared_;

            case DirectionSlot::Invalid:
            default:
                return queue_shared_;
        }
    }

    const std::deque<Link::LinkTransmission>&
    Link::queueForSlot(DirectionSlot slot) const noexcept
    {
        switch (slot) {
            case DirectionSlot::AB:
                return queue_ab_;

            case DirectionSlot::BA:
                return queue_ba_;

            case DirectionSlot::Shared:
                return queue_shared_;

            case DirectionSlot::Invalid:
            default:
                return queue_shared_;
        }
    }

    Link::DirectionSlot Link::getQueueSlot(
        int from,
        int to
    ) const noexcept
    {
        const DirectionSlot direction = getDirectionSlot(from, to);

        switch (mode_) {
            case LinkMode::FULL_DUPLEX:
                return direction;

            case LinkMode::HALF_DUPLEX:
                if (
                    direction == DirectionSlot::AB ||
                    direction == DirectionSlot::BA
                ) {
                    return DirectionSlot::Shared;
                }

                return DirectionSlot::Invalid;

            case LinkMode::SIMPLEX:
                return direction == DirectionSlot::AB
                    ? DirectionSlot::AB
                    : DirectionSlot::Invalid;

            default:
                return DirectionSlot::Invalid;
        }
    }
} // namespace kns