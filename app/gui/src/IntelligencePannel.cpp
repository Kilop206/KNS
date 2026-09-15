#include "IntelligencePanel.hpp"

#include <imgui.h>

#include <string>
#include <utility>

namespace kns::app::gui {

namespace {

const char* modeToString(
    kns::intelligence::AnalysisMode mode
)
{
    switch (mode) {
        case kns::intelligence::AnalysisMode::Quick:
            return "Quick";

        case kns::intelligence::AnalysisMode::Detailed:
            return "Detailed";

        case kns::intelligence::AnalysisMode::Deep:
            return "Deep";
    }

    return "Detailed";
}

void renderScore(double score)
{
    ImGui::Text(
        "Network Score: %.1f / 100",
        score
    );

    const float normalized =
        static_cast<float>(
            score / 100.0
        );

    ImGui::ProgressBar(
        normalized,
        ImVec2(-1.0f, 0.0f)
    );
}

} // namespace

IntelligencePanel::IntelligencePanel(
    intelligence::IntelligenceClientConfig config
)
    : service_(
        intelligence::IntelligenceClient(
            std::move(config)
        )
    )
{
}

void IntelligencePanel::render(
    const std::optional<
        kns::analysis::NetworkAnalysis
    >& analysis
)
{
    service_.update();

    ImGui::Begin("KNS Intelligence");

    const auto state =
        service_.getState();

    ImGui::TextDisabled(
        "State: %s",
        stateToString(state)
    );

    ImGui::Separator();

    switch (state) {
        case intelligence::IntelligenceServiceState::Idle:
            renderIdle(analysis);
            break;

        case intelligence::IntelligenceServiceState::Analyzing:
            renderAnalyzing();
            break;

        case intelligence::IntelligenceServiceState::Success:
            renderSuccess();
            break;

        case intelligence::IntelligenceServiceState::Error:
            renderError();
            break;
    }

    ImGui::End();
}

void IntelligencePanel::renderIdle(
    const std::optional<
        kns::analysis::NetworkAnalysis
    >& analysis
)
{
    ImGui::TextWrapped(
        "Analyze the current network using "
        "KNS Intelligence."
    );

    ImGui::Spacing();

    ImGui::Text("Analysis mode");

    const char* preview =
        modeToString(selected_mode_);

    if (
        ImGui::BeginCombo(
            "##analysis_mode",
            preview
        )
    ) {
        const auto renderModeOption =
            [this](
                const char* label,
                kns::intelligence::AnalysisMode mode
            )
            {
                const bool selected =
                    selected_mode_ == mode;

                if (
                    ImGui::Selectable(
                        label,
                        selected
                    )
                ) {
                    selected_mode_ =
                        mode;
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            };

        renderModeOption(
            "Quick",
            kns::intelligence::AnalysisMode::Quick
        );

        renderModeOption(
            "Detailed",
            kns::intelligence::AnalysisMode::Detailed
        );

        renderModeOption(
            "Deep",
            kns::intelligence::AnalysisMode::Deep
        );

        ImGui::EndCombo();
    }

    ImGui::Spacing();

    if (!analysis.has_value()) {
        ImGui::TextDisabled(
            "Load a topology before running "
            "KNS Intelligence."
        );

        return;
    }

    if (
        ImGui::Button(
            "Analyze with KNS Intelligence"
        )
    ) {
        service_.startAnalysis(
            *analysis,
            selected_mode_
        );
    }
}

void IntelligencePanel::renderAnalyzing()
{
    ImGui::Text(
        "Analyzing network..."
    );

    ImGui::Spacing();

    const float progress =
        static_cast<float>(
            ImGui::GetTime()
        );

    const float animated =
        0.15f +
        0.70f *
            static_cast<float>(
                (static_cast<int>(
                    progress * 10.0f
                ) % 10)
            ) /
            10.0f;

    ImGui::ProgressBar(
        animated,
        ImVec2(-1.0f, 0.0f),
        "Waiting for KNS Intelligence..."
    );

    ImGui::Spacing();

    ImGui::TextDisabled(
        "The simulation interface remains available "
        "while the analysis runs."
    );
}

void IntelligencePanel::renderSuccess()
{
    const auto response =
        service_.getResponse();

    if (!response.has_value()) {
        ImGui::Text(
            "Analysis completed, but no response "
            "is available."
        );

        if (ImGui::Button("Reset")) {
            service_.reset();
        }

        return;
    }

    renderResponse(
        *response
    );

    ImGui::Separator();

    if (
        ImGui::Button(
            "Analyze Again"
        )
    ) {
        service_.reset();
    }
}

void IntelligencePanel::renderError()
{
    ImGui::Text(
        "KNS Intelligence request failed."
    );

    ImGui::Spacing();

    const std::string error =
        service_.getError();

    ImGui::TextWrapped(
        "%s",
        error.c_str()
    );

    ImGui::Spacing();

    if (ImGui::Button("Try Again")) {
        service_.reset();
    }
}

void IntelligencePanel::renderResponse(
    const kns::intelligence::IntelligenceResponse& response
)
{
    renderScore(
        response.network_score
    );

    ImGui::Spacing();

    if (!response.analysis_id.empty()) {
        ImGui::TextDisabled(
            "Analysis ID: %s",
            response.analysis_id.c_str()
        );
    }

    ImGui::Separator();

    ImGui::Text("Summary");

    ImGui::Spacing();

    ImGui::TextWrapped(
        "%s",
        response.summary.c_str()
    );

    ImGui::Spacing();
    ImGui::Separator();

    renderFindings(
        response
    );

    ImGui::Spacing();
    ImGui::Separator();

    renderRecommendations(
        response
    );
}

void IntelligencePanel::renderFindings(
    const kns::intelligence::IntelligenceResponse& response
)
{
    ImGui::Text(
        "Findings (%zu)",
        response.findings.size()
    );

    if (response.findings.empty()) {
        ImGui::TextDisabled(
            "No findings reported."
        );

        return;
    }

    for (
        std::size_t i = 0;
        i < response.findings.size();
        ++i
    ) {
        const auto& finding =
            response.findings[i];

        ImGui::PushID(
            static_cast<int>(i)
        );

        const std::string header =
            std::string(
                severityToString(
                    finding.severity
                )
            ) +
            " - " +
            finding.title;

        if (
            ImGui::TreeNode(
                "finding",
                "%s",
                header.c_str()
            )
        ) {
            if (!finding.category.empty()) {
                ImGui::TextDisabled(
                    "Category: %s",
                    finding.category.c_str()
                );
            }

            ImGui::TextWrapped(
                "%s",
                finding.description.c_str()
            );

            if (
                !finding.affected_nodes.empty()
            ) {
                ImGui::Spacing();
                ImGui::Text("Affected nodes:");

                for (
                    const int node :
                    finding.affected_nodes
                ) {
                    ImGui::SameLine();

                    ImGui::Text(
                        "%d",
                        node
                    );
                }
            }

            if (
                !finding.affected_links.empty()
            ) {
                ImGui::Spacing();
                ImGui::Text(
                    "Affected links:"
                );

                for (
                    const auto& link :
                    finding.affected_links
                ) {
                    ImGui::BulletText(
                        "%d -> %d",
                        link.from,
                        link.to
                    );
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void IntelligencePanel::renderRecommendations(
    const kns::intelligence::IntelligenceResponse& response
)
{
    ImGui::Text(
        "Recommendations (%zu)",
        response.recommendations.size()
    );

    if (
        response.recommendations.empty()
    ) {
        ImGui::TextDisabled(
            "No recommendations reported."
        );

        return;
    }

    for (
        std::size_t i = 0;
        i < response.recommendations.size();
        ++i
    ) {
        const auto& recommendation =
            response.recommendations[i];

        ImGui::PushID(
            static_cast<int>(i)
        );

        const std::string header =
            recommendation.priority +
            " - " +
            recommendation.title;

        if (
            ImGui::TreeNode(
                "recommendation",
                "%s",
                header.c_str()
            )
        ) {
            ImGui::TextWrapped(
                "%s",
                recommendation.description.c_str()
            );

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

const char*
IntelligencePanel::severityToString(
    kns::intelligence::FindingSeverity severity
)
{
    switch (severity) {
        case kns::intelligence::FindingSeverity::Info:
            return "Info";

        case kns::intelligence::FindingSeverity::Low:
            return "Low";

        case kns::intelligence::FindingSeverity::Medium:
            return "Medium";

        case kns::intelligence::FindingSeverity::High:
            return "High";

        case kns::intelligence::FindingSeverity::Critical:
            return "Critical";
    }

    return "Unknown";
}

const char*
IntelligencePanel::stateToString(
    intelligence::IntelligenceServiceState state
)
{
    switch (state) {
        case intelligence::IntelligenceServiceState::Idle:
            return "Idle";

        case intelligence::IntelligenceServiceState::Analyzing:
            return "Analyzing";

        case intelligence::IntelligenceServiceState::Success:
            return "Completed";

        case intelligence::IntelligenceServiceState::Error:
            return "Error";
    }

    return "Unknown";
}

}