#include "intelligence/IntelligenceService.hpp"

#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

#include "intelligence/IntelligenceRequestBuilder.hpp"

namespace kns::app::intelligence {

IntelligenceService::IntelligenceService(
    IntelligenceClient client
)
    : client_(std::move(client))
{
}

IntelligenceService::~IntelligenceService()
{
    if (future_.valid()) {
        future_.wait();
    }
}

void IntelligenceService::startAnalysis(
    const kns::analysis::NetworkAnalysis& analysis,
    std::uint64_t topologyRevision,
    kns::intelligence::AnalysisMode mode
)
{
    const auto request =
        kns::intelligence::
            IntelligenceRequestBuilder::build(
                analysis,
                mode
            );

    {
        std::scoped_lock lock(mutex_);

        if (
            state_ ==
            IntelligenceServiceState::Analyzing
        ) {
            throw std::runtime_error(
                "An intelligence analysis is already running"
            );
        }

        active_topology_revision_ =
            topologyRevision;

        state_ =
            IntelligenceServiceState::Analyzing;

        response_.reset();
        error_.clear();
    }

    future_ =
        std::async(
            std::launch::async,
            [client = client_,
             request]() mutable
            {
                return client.analyze(
                    request
                );
            }
        );
}

std::uint64_t
IntelligenceService::getCompletedTopologyRevision() const
{
    std::scoped_lock lock(mutex_);

    return completed_topology_revision_;
}

void IntelligenceService::update()
{
    {
        std::scoped_lock lock(mutex_);

        if (
            state_ !=
            IntelligenceServiceState::Analyzing
        ) {
            return;
        }
    }

    if (!future_.valid()) {
        setError(
            "Intelligence analysis future is invalid"
        );

        return;
    }

    const auto status =
        future_.wait_for(
            std::chrono::seconds(0)
        );

    if (
        status !=
        std::future_status::ready
    ) {
        return;
    }

    try {
        auto response =
            future_.get();

        std::scoped_lock lock(mutex_);

        response_ =
            std::move(response);

        completed_topology_revision_ =
            active_topology_revision_;

        error_.clear();

        state_ =
            IntelligenceServiceState::Success;
    }
    catch (const std::exception& e) {
        setError(
            e.what()
        );
    }
    catch (...) {
        setError(
            "Unknown intelligence analysis error"
        );
    }
}

IntelligenceServiceState
IntelligenceService::getState() const
{
    std::scoped_lock lock(mutex_);

    return state_;
}

bool IntelligenceService::isAnalyzing() const
{
    return
        getState() ==
        IntelligenceServiceState::Analyzing;
}

std::optional<
    kns::intelligence::IntelligenceResponse
>
IntelligenceService::getResponse() const
{
    std::scoped_lock lock(mutex_);

    return response_;
}

std::string
IntelligenceService::getError() const
{
    std::scoped_lock lock(mutex_);

    return error_;
}

void IntelligenceService::reset()
{
    std::scoped_lock lock(mutex_);

    if (
        state_ ==
        IntelligenceServiceState::Analyzing
    ) {
        return;
    }

    state_ =
        IntelligenceServiceState::Idle;

    response_.reset();
    error_.clear();
}

void IntelligenceService::setError(
    const std::string& message
)
{
    std::scoped_lock lock(mutex_);

    error_ =
        message;

    response_.reset();

    state_ =
        IntelligenceServiceState::Error;
}

}