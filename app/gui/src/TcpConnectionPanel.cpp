#include "gui/include/TcpConnectionPanel.hpp"

#include <algorithm>

#include "imgui.h"

#include "engine/core/SimulationEngine.hpp"
#include "network/transport/tcp/TCPSession.hpp"

namespace gui {
    namespace {
        constexpr int kMinimumPort = 0;
        constexpr int kMaximumPort = 65535;

        const char* stateLabel(kns::TCPState state) noexcept
        {
            switch (state) {
                case kns::TCPState::CLOSED: return "CLOSED";
                case kns::TCPState::LISTEN: return "LISTEN";
                case kns::TCPState::SYN_SENT: return "SYN_SENT";
                case kns::TCPState::SYN_RECEIVED: return "SYN_RECEIVED";
                case kns::TCPState::ESTABLISHED: return "ESTABLISHED";
                case kns::TCPState::FIN_WAIT_1: return "FIN_WAIT_1";
                case kns::TCPState::FIN_WAIT_2: return "FIN_WAIT_2";
                case kns::TCPState::CLOSE_WAIT: return "CLOSE_WAIT";
                case kns::TCPState::LAST_ACK: return "LAST_ACK";
                case kns::TCPState::TIME_WAIT: return "TIME_WAIT";
                case kns::TCPState::CLOSING: return "CLOSING";
            }

            return "UNKNOWN";
        }
    } // namespace

    std::optional<TcpConnectionAction> TcpConnectionPanel::render(
        const kns::SimulationEngine& engine
    )
    {
        std::optional<TcpConnectionAction> action;

        ImGui::Begin("TCP Connections");

        const int node_count = engine.getTopology().size();

        if (node_count < 2) {
            ImGui::TextDisabled("At least two nodes are required.");
        } else {
            source_node_ = std::clamp(source_node_, 0, node_count - 1);
            destination_node_ = std::clamp(destination_node_, 0, node_count - 1);
            source_port_ = std::clamp(source_port_, kMinimumPort, kMaximumPort);
            destination_port_ = std::clamp(
                destination_port_,
                kMinimumPort,
                kMaximumPort
            );

            ImGui::InputInt("Source node", &source_node_);
            ImGui::InputInt("Destination node", &destination_node_);
            ImGui::InputInt("Source port", &source_port_);
            ImGui::InputInt("Destination port", &destination_port_);

            source_node_ = std::clamp(source_node_, 0, node_count - 1);
            destination_node_ = std::clamp(destination_node_, 0, node_count - 1);
            source_port_ = std::clamp(source_port_, kMinimumPort, kMaximumPort);
            destination_port_ = std::clamp(
                destination_port_,
                kMinimumPort,
                kMaximumPort
            );

            const bool valid_nodes = source_node_ != destination_node_;

            ImGui::BeginDisabled(!valid_nodes);

            if (ImGui::Button("Open TCP Session")) {
                action = TcpConnectionAction{
                    TcpConnectionActionType::Open,
                    source_node_,
                    destination_node_,
                    static_cast<std::uint16_t>(source_port_),
                    static_cast<std::uint16_t>(destination_port_),
                    0
                };
            }

            ImGui::EndDisabled();

            if (!valid_nodes) {
                ImGui::TextDisabled("Source and destination must differ.");
            }
        }

        ImGui::Separator();

        if (ImGui::BeginTable(
                "TcpConnectionTable",
                5,
                ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Session");
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Destination");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const auto& [session_id, session] : engine.getTCPSessions()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%llu", static_cast<unsigned long long>(session_id));
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d:%u", session.getSource(), session.getClientConnection().getLocalPort());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d:%u", session.getDestination(), session.getClientConnection().getRemotePort());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(stateLabel(session.getState()));
                ImGui::TableSetColumnIndex(4);

                ImGui::PushID(static_cast<int>(session_id));

                if (ImGui::SmallButton("Cancel")) {
                    action = TcpConnectionAction{
                        TcpConnectionActionType::Cancel,
                        0,
                        0,
                        0,
                        0,
                        session_id
                    };
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::End();

        return action;
    }

} // namespace gui
