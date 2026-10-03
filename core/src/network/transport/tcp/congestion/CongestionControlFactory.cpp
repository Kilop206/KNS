#include "network/transport/tcp/congestion/CongestionControlFactory.hpp"

#include <stdexcept>

#include "network/transport/tcp/congestion/CubicCongestionControl.hpp"
#include "network/transport/tcp/congestion/NewRenoCongestionControl.hpp"
#include "network/transport/tcp/congestion/RenoCongestionControl.hpp"
#include "network/transport/tcp/congestion/TahoeCongestionControl.hpp"

namespace kns
{
    std::unique_ptr<CongestionControl>
    CongestionControlFactory::create(
        CongestionControlType type,
        std::uint32_t mss,
        std::uint32_t initial_ssthresh
    )
    {
        switch (type)
        {
        case CongestionControlType::TAHOE:
            return std::make_unique<TahoeCongestionControl>(
                mss,
                initial_ssthresh
            );

        case CongestionControlType::RENO:
            return std::make_unique<RenoCongestionControl>(
                mss,
                initial_ssthresh
            );

        case CongestionControlType::NEW_RENO:
            return std::make_unique<NewRenoCongestionControl>(
                mss,
                initial_ssthresh
            );

        case CongestionControlType::CUBIC:
            return std::make_unique<CubicCongestionControl>(
                mss,
                initial_ssthresh
            );
        }

        throw std::invalid_argument(
            "Unsupported congestion control type"
        );
    }
}