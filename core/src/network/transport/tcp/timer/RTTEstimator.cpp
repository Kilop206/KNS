#include "network/transport/tcp/timer/RTTEstimator.hpp"

#include <algorithm>
#include <cmath>

namespace kns {

    void RTTEstimator::update(
        double measured_rtt
    ) noexcept
    {
        if (!std::isfinite(measured_rtt) ||
            measured_rtt <= 0.0) {
            return;
        }

        if (!initialized_) {
            srtt_ = measured_rtt;
            rttvar_ = measured_rtt / 2.0;

            rto_ = srtt_ + K * rttvar_;

            rto_ = std::clamp(
                rto_,
                MIN_RTO,
                MAX_RTO
            );

            initialized_ = true;
            return;
        }

        rttvar_ =
            (1.0 - BETA) * rttvar_ +
            BETA * std::abs(srtt_ - measured_rtt);

        srtt_ =
            (1.0 - ALPHA) * srtt_ +
            ALPHA * measured_rtt;

        rto_ =
            srtt_ + K * rttvar_;

        rto_ = std::clamp(
            rto_,
            MIN_RTO,
            MAX_RTO
        );
    }

    void RTTEstimator::reset() noexcept
    {
        srtt_ = 0.0;
        rttvar_ = 0.0;
        rto_ = INITIAL_RTO;
        initialized_ = false;
    }

} // namespace kns