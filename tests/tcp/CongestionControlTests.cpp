#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "network/transport/tcp/congestion/CongestionControl.hpp"
#include "network/transport/tcp/congestion/CongestionControlType.hpp"

using namespace kns;

namespace
{
    class TestCongestionControl final : public CongestionControl
    {
    public:
        TestCongestionControl(
            std::uint32_t cwnd,
            std::uint32_t ssthresh,
            std::uint32_t mss
        )
            : cwnd_(cwnd),
              ssthresh_(ssthresh),
              mss_(mss)
        {
        }

        std::uint32_t getCwnd() const noexcept override
        {
            return cwnd_;
        }

        std::uint32_t getSsthresh() const noexcept override
        {
            return ssthresh_;
        }

        std::uint32_t getMss() const noexcept override
        {
            return mss_;
        }

        bool canSend(
            std::uint32_t bytes_in_flight,
            std::uint32_t segment_size
        ) const noexcept override
        {
            return bytes_in_flight <= cwnd_ &&
                   segment_size <= cwnd_ - bytes_in_flight;
        }

        void onAck(
            std::uint32_t acknowledged_bytes
        ) noexcept override
        {
            acknowledged_bytes_ += acknowledged_bytes;
        }

        void onLoss() noexcept override
        {
            loss_count_++;
        }

        void reset() noexcept override
        {
            acknowledged_bytes_ = 0;
            loss_count_ = 0;
        }

        std::uint32_t acknowledgedBytes() const noexcept
        {
            return acknowledged_bytes_;
        }

        std::uint32_t lossCount() const noexcept
        {
            return loss_count_;
        }

    private:
        std::uint32_t cwnd_;
        std::uint32_t ssthresh_;
        std::uint32_t mss_;

        std::uint32_t acknowledged_bytes_ = 0;
        std::uint32_t loss_count_ = 0;
    };
}

TEST_CASE(
    "CongestionControl exposes the congestion state",
    "[tcp][congestion]"
)
{
    TestCongestionControl control(
        12000,
        64000,
        1000
    );

    REQUIRE(control.getCwnd() == 12000);
    REQUIRE(control.getSsthresh() == 64000);
    REQUIRE(control.getMss() == 1000);
}

TEST_CASE(
    "CongestionControl enforces the congestion window contract",
    "[tcp][congestion]"
)
{
    TestCongestionControl control(
        12000,
        64000,
        1000
    );

    REQUIRE(control.canSend(0, 1000));
    REQUIRE(control.canSend(11000, 1000));
    REQUIRE(control.canSend(12000, 0));

    REQUIRE_FALSE(control.canSend(11500, 1000));
    REQUIRE_FALSE(control.canSend(12001, 1));
}

TEST_CASE(
    "CongestionControl receives ACK and loss notifications",
    "[tcp][congestion]"
)
{
    TestCongestionControl control(
        12000,
        64000,
        1000
    );

    control.onAck(2000);
    control.onAck(1000);

    REQUIRE(control.acknowledgedBytes() == 3000);
    REQUIRE(control.lossCount() == 0);

    control.onLoss();

    REQUIRE(control.lossCount() == 1);

    control.reset();

    REQUIRE(control.acknowledgedBytes() == 0);
    REQUIRE(control.lossCount() == 0);
}

TEST_CASE(
    "CongestionControlType defines supported algorithms",
    "[tcp][congestion]"
)
{
    REQUIRE(
        CongestionControlType::TAHOE !=
        CongestionControlType::RENO
    );

    REQUIRE(
        CongestionControlType::RENO !=
        CongestionControlType::NEW_RENO
    );

    REQUIRE(
        CongestionControlType::NEW_RENO !=
        CongestionControlType::CUBIC
    );
}