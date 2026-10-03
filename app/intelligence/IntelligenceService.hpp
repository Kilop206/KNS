#pragma once

#include <cstdint>
#include <future>
#include <mutex>
#include <optional>
#include <string>

#include "analysis/NetworkAnalysis.hpp"
#include "intelligence/IntelligenceClient.hpp"
#include "intelligence/IntelligenceResponse.hpp"

namespace kns::app::intelligence {

enum class IntelligenceServiceState {
    Idle,
    Analyzing,
    Success,
    Error
};

class IntelligenceService {
public:
    explicit IntelligenceService(
        IntelligenceClient client
    );

    ~IntelligenceService();

    IntelligenceService(
        const IntelligenceService&
    ) = delete;

    IntelligenceService& operator=(
        const IntelligenceService&
    ) = delete;

    void startAnalysis(
        const kns::analysis::NetworkAnalysis& analysis,
        std::uint64_t topologyRevision,
        kns::intelligence::AnalysisMode mode =
            kns::intelligence::AnalysisMode::Detailed
    );

    void update();

    [[nodiscard]]
    IntelligenceServiceState getState() const;

    [[nodiscard]]
    bool isAnalyzing() const;

    [[nodiscard]]
    std::optional<
        kns::intelligence::IntelligenceResponse
    > getResponse() const;

    [[nodiscard]]
    std::string getError() const;

    void reset();

    [[nodiscard]]
    std::uint64_t getCompletedTopologyRevision() const;

private:
    void setError(
        const std::string& message
    );

    mutable std::mutex mutex_;

    IntelligenceClient client_;

    IntelligenceServiceState state_ =
        IntelligenceServiceState::Idle;

    std::optional<
        kns::intelligence::IntelligenceResponse
    > response_;

    std::string error_;

    std::future<
        kns::intelligence::IntelligenceResponse
    > future_;

    std::uint64_t active_topology_revision_ = 0;
    std::uint64_t completed_topology_revision_ = 0;
};

}