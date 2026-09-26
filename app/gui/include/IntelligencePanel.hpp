#pragma once

#include <optional>
#include <array>

#include "analysis/NetworkAnalysis.hpp"
#include "intelligence/IntelligenceService.hpp"
#include "intelligence/ChatService.hpp"

namespace kns::app::gui {

class IntelligencePanel {
public:
    explicit IntelligencePanel(
        intelligence::IntelligenceClientConfig config = {}
    );

    void render(
        const std::optional<
            kns::analysis::NetworkAnalysis
        >& analysis,
        std::uint64_t topologyRevision
    );

private:
    void renderAnalysis(const std::optional<kns::analysis::NetworkAnalysis>& analysis,
                        std::uint64_t topologyRevision);
    void renderChat(const std::optional<kns::analysis::NetworkAnalysis>& analysis);
    intelligence::ChatService chat_;
    std::array<char, 4097> chat_input_{};
    std::uint64_t chat_revision_ = 0;
    std::size_t displayed_messages_ = 0;

    static const char* severityToString(
        kns::intelligence::FindingSeverity severity
    );

    static const char* stateToString(
        intelligence::IntelligenceServiceState state
    );

    void renderIdle(
        const std::optional<
            kns::analysis::NetworkAnalysis
        >& analysis,
        std::uint64_t topologyRevision
    );

    void renderAnalyzing();

    void renderSuccess();

    void renderError();

    void renderResponse(
        const kns::intelligence::IntelligenceResponse& response
    );

    void renderFindings(
        const kns::intelligence::IntelligenceResponse& response
    );

    void renderRecommendations(
        const kns::intelligence::IntelligenceResponse& response
    );

    intelligence::IntelligenceService service_;

    kns::intelligence::AnalysisMode selected_mode_ =
        kns::intelligence::AnalysisMode::Detailed;
};

}
