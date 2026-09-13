#include "../include/TcpCongestionPanel.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "../../../core/include/network/transport/tcp/TCPConnection.hpp"
#include "../../../core/include/network/transport/tcp/TCPSession.hpp"
#include "../../../core/include/network/transport/tcp/congestion/CongestionControlType.hpp"
#include "../../../core/include/network/transport/tcp/congestion/CongestionControl.hpp"
#include "../../../core/include/network/transport/tcp/congestion/RenoCongestionControl.hpp"
#include "../../../core/include/network/transport/tcp/congestion/NewRenoCongestionControl.hpp"
#include "../../../core/include/network/transport/tcp/congestion/TahoeCongestionControl.hpp"
#include "../../../core/include/network/transport/tcp/congestion/CubicCongestionControl.hpp"
#include "../include/TranslationService.hpp"

namespace gui {

    namespace {

        const char* congestionControlTypeToString(
            kns::CongestionControlType type
        ) noexcept
        {
            switch (type) {
                case kns::CongestionControlType::TAHOE:
                    return "Tahoe";

                case kns::CongestionControlType::RENO:
                    return "Reno";

                case kns::CongestionControlType::NEW_RENO:
                    return "NewReno";

                case kns::CongestionControlType::CUBIC:
                    return "CUBIC";
            }

            return "Unknown";
        }

        bool congestionControlInFastRecovery(
            const kns::CongestionControl& control
        ) noexcept
        {
            if (const auto* reno =
                dynamic_cast<const kns::RenoCongestionControl*>(
                    &control
                ))
            {
                return reno->inFastRecovery();
            }

            if (const auto* new_reno =
                dynamic_cast<const kns::NewRenoCongestionControl*>(
                    &control
                ))
            {
                return new_reno->inFastRecovery();
            }

            if (const auto* cubic =
                dynamic_cast<const kns::CubicCongestionControl*>(
                    &control
                ))
            {
                return cubic->inFastRecovery();
            }

            return false;
        }

        void renderCongestionChart(
            const kns::TCPConnection& connection,
            TranslationService& translations
        )
        {
            const auto& history =
                connection.getCongestionHistory();

            if (history.empty()) {
                ImGui::TextDisabled(
                    "%s",
                    translations.translate(
                        "No congestion history available."
                    ).c_str()
                );

                return;
            }

            std::vector<float> cwnd_values;
            std::vector<float> ssthresh_values;

            cwnd_values.reserve(history.size());
            ssthresh_values.reserve(history.size());

            for (const auto& sample : history) {
                cwnd_values.push_back(
                    static_cast<float>(sample.cwnd)
                );

                ssthresh_values.push_back(
                    static_cast<float>(sample.ssthresh)
                );
            }

            const auto max_cwnd =
                *std::max_element(
                    cwnd_values.begin(),
                    cwnd_values.end()
                );

            const auto max_ssthresh =
                *std::max_element(
                    ssthresh_values.begin(),
                    ssthresh_values.end()
                );

            const float max_value =
                std::max(
                    max_cwnd,
                    max_ssthresh
                );

            const float chart_max =
                max_value > 0.0f
                    ? max_value * 1.10f
                    : 1.0f;

            ImGui::Separator();

            ImGui::TextUnformatted(
                translations.translate(
                    "Congestion Window History"
                ).c_str()
            );

            ImGui::PlotLines(
                "cwnd",
                cwnd_values.data(),
                static_cast<int>(cwnd_values.size()),
                0,
                nullptr,
                0.0f,
                chart_max,
                ImVec2(
                    -1.0f,
                    180.0f
                )
            );

            ImGui::PlotLines(
                "ssthresh",
                ssthresh_values.data(),
                static_cast<int>(ssthresh_values.size()),
                0,
                nullptr,
                0.0f,
                chart_max,
                ImVec2(
                    -1.0f,
                    180.0f
                )
            );

            const auto& first =
                history.front();

            const auto& last =
                history.back();

            ImGui::Text(
                "%s: %u",
                translations.translate("Samples").c_str(),
                static_cast<unsigned>(
                    history.size()
                )
            );

            ImGui::Text(
                "%s: t=%.3f s",
                translations.translate("First sample").c_str(),
                first.timestamp
            );

            ImGui::Text(
                "%s: t=%.3f s",
                translations.translate("Last sample").c_str(),
                last.timestamp
            );
        }

    }

    void TcpCongestionPanel::render(
        const kns::SimulationEngine& engine,
        TranslationService& translations
    )
    {
        const std::string window_label = translations.label(
            "TCP Congestion Control",
            "tcp-congestion-window"
        );
        ImGui::Begin(window_label.c_str());

        const auto& sessions =
            engine.getTCPSessions();

        if (sessions.empty()) {
            ImGui::TextDisabled(
                "%s",
                translations.translate("No TCP sessions available.").c_str()
            );

            ImGui::End();
            return;
        }

        static std::uint64_t selected_session_id = 0;

        if (!sessions.contains(
            selected_session_id
        )) {
            selected_session_id =
                sessions.begin()->first;
        }

        const auto selected =
            sessions.find(
                selected_session_id
            );

        if (selected == sessions.end()) {
            ImGui::TextDisabled(
                "%s",
                translations.translate(
                    "Selected TCP session is unavailable."
                ).c_str()
            );

            ImGui::End();
            return;
        }

        const std::uint64_t session_id =
            selected->first;

        ImGui::Text(
            "%s: %llu",
            translations.translate("Session").c_str(),
            static_cast<unsigned long long>(
                session_id
            )
        );

        ImGui::SameLine();

        if (ImGui::BeginCombo(
            "##TCPCongestionSession",
            std::to_string(
                session_id
            ).c_str()
        ))
        {
            for (const auto& [id, session] : sessions)
            {
                const bool is_selected =
                    id == selected_session_id;

                const bool selectable =
                    ImGui::Selectable(
                        std::to_string(id).c_str(),
                        is_selected
                    );

                if (selectable) {
                    selected_session_id = id;
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }

                static_cast<void>(session);
            }

            ImGui::EndCombo();
        }

        const auto& session =
            selected->second;

        const auto& connection =
            session.getClientConnection();

        const auto& congestion =
            connection.getCongestionControl();

        ImGui::Separator();

        ImGui::Text(
            "%s: %s",
            translations.translate("Algorithm").c_str(),
            congestionControlTypeToString(
                connection.getCongestionControlType()
            )
        );

        ImGui::Text(
            "MSS: %u %s",
            congestion.getMss(),
            translations.translate("bytes").c_str()
        );

        ImGui::Text(
            "cwnd: %u %s",
            congestion.getCwnd(),
            translations.translate("bytes").c_str()
        );

        ImGui::Text(
            "ssthresh: %u %s",
            congestion.getSsthresh(),
            translations.translate("bytes").c_str()
        );

        ImGui::Text(
            "%s: %u",
            translations.translate("Send unacknowledged").c_str(),
            connection.getSendUnacknowledged()
        );

        ImGui::Text(
            "%s: %u",
            translations.translate("Send next").c_str(),
            connection.getSendNext()
        );

        const std::uint32_t bytes_in_flight =
            connection.getSendNext() -
            connection.getSendUnacknowledged();

        ImGui::Text(
            "%s: %u",
            translations.translate("Bytes in flight").c_str(),
            bytes_in_flight
        );

        ImGui::Text(
            "%s: %u",
            translations.translate("Duplicate ACKs").c_str(),
            connection.getDuplicateAckCount()
        );

        ImGui::Text(
            "%s: %s",
            translations.translate("State").c_str(),
            congestionControlInFastRecovery(
                congestion
            )
                ? translations.translate("Fast Recovery").c_str()
                : translations.translate("Normal").c_str()
        );

        renderCongestionChart(
            connection,
            translations
        );

        ImGui::End();
    }

}
