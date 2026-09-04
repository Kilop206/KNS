#include "network/transport/tcp/timer/RTOManager.hpp"

#include <algorithm>

namespace kns {

    double RTOManager::currentRTO() const noexcept
    {
        return std::min(
            estimator_.getRTO() * backoff_,
            RTTEstimator::MAX_RTO
        );
    }

    double RTOManager::onTimeout() noexcept
    {
        backoff_ =
            std::min(
                backoff_ * 2.0,
                MAX_BACKOFF
            );

        return currentRTO();
    }

    void RTOManager::onAcknowledgement(
        double measured_rtt
    ) noexcept
    {
        estimator_.update(
            measured_rtt
        );

        backoff_ = INITIAL_BACKOFF;
    }

    void RTOManager::reset() noexcept
    {
        estimator_.reset();
        backoff_ = INITIAL_BACKOFF;
    }

} // namespace kns