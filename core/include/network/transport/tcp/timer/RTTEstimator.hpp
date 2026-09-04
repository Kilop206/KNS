#pragma once

namespace kns {

    class RTTEstimator {
    public:
        static constexpr double INITIAL_RTO = 1.0;
        static constexpr double MIN_RTO = 0.2;
        static constexpr double MAX_RTO = 60.0;

        static constexpr double ALPHA = 1.0 / 8.0;
        static constexpr double BETA = 1.0 / 4.0;
        static constexpr double K = 4.0;

        RTTEstimator() noexcept = default;

        void update(double measured_rtt) noexcept;

        double getSRTT() const noexcept {
            return srtt_;
        }

        double getRTTVAR() const noexcept {
            return rttvar_;
        }

        double getRTO() const noexcept {
            return rto_;
        }

        bool hasSample() const noexcept {
            return initialized_;
        }

        void reset() noexcept;

    private:
        double srtt_ = 0.0;
        double rttvar_ = 0.0;
        double rto_ = INITIAL_RTO;
        bool initialized_ = false;
    };

} // namespace kns