#pragma once

#include "network/transport/tcp/timer/RTTEstimator.hpp"

namespace kns {

    class RTOManager {
    public:
        static constexpr double INITIAL_BACKOFF = 1.0;
        static constexpr double MAX_BACKOFF = 64.0;

        RTOManager() noexcept = default;

        double currentRTO() const noexcept;

        double onTimeout() noexcept;

        void onAcknowledgement(
            double measured_rtt
        ) noexcept;

        void reset() noexcept;

        double getBackoff() const noexcept {
            return backoff_;
        }

        const RTTEstimator& getEstimator() const noexcept {
            return estimator_;
        }

        RTTEstimator& getEstimator() noexcept {
            return estimator_;
        }

    private:
        RTTEstimator estimator_;
        double backoff_ = INITIAL_BACKOFF;
    };

} // namespace kns