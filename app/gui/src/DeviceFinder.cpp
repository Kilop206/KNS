#include "DeviceFinder.hpp"
#include "DeviceIcons.hpp"
#include "TranslationService.hpp"
#include "network/Topology.hpp"
#include "imgui.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string>

namespace gui {
    namespace {
        std::string lower(std::string_view text) {
            std::string result(text);
            for (auto& c : result)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c + ('a' - 'A'));
            return result;
        }

        bool whitespace(char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        }
    }

    DeviceSearchResults findDevices(const kns::Topology& topology, std::string_view query,
                                    std::size_t limit) {
        if (limit == 0 || limit > 256)
            throw std::invalid_argument("Device search limit must be between 1 and 256");
        std::vector<std::string> terms;
        std::string token;
        for (char c : query) {
            if (whitespace(c)) {
                if (!token.empty())
                    terms.push_back(lower(token));
                token.clear();
            } else {
                token += c;
            }
        }
        if (!token.empty())
            terms.push_back(lower(token));

        DeviceSearchResults result;
        for (int id = 0; id < topology.size(); ++id) {
            const auto* node = topology.getNode(id);
            if (!node || !node->isActive())
                continue;
            const auto& info = node->getDeviceInfo();
            std::string fields = node->getLabel() + " " + std::string(kns::toString(info.type)) +
                " " + info.external_id + " " + info.mac;
            for (const auto& address : info.addresses)
                fields += " " + address;
            fields = lower(fields);
            const bool matches = std::all_of(terms.begin(), terms.end(), [&](const auto& term) {
                if (term.starts_with('#')) {
                    int requested = -1;
                    const auto parsed = std::from_chars(term.data() + 1,
                                                        term.data() + term.size(), requested);
                    return parsed.ec == std::errc{} && parsed.ptr == term.data() + term.size() &&
                           requested == id;
                }
                if (term.starts_with("type:"))
                    return term.substr(5) == kns::toString(info.type);
                return fields.find(term) != std::string::npos;
            });
            if (matches) {
                ++result.total;
                if (result.nodes.size() < limit)
                    result.nodes.push_back(id);
            }
        }
        return result;
    }

    std::optional<int> DeviceFinder::render(const kns::Topology& topology, TranslationService& tr) {
        const auto* viewport = ImGui::GetMainViewport();
        // BeginPopup auto-resizes by default. A fill-height results table must
        // have a fixed viewport-bounded window every frame to avoid size feedback.
        ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({std::clamp(viewport->WorkSize.x - 32, 1.0f, 620.0f),
                                  std::clamp(viewport->WorkSize.y - 32, 1.0f, 380.0f)},
                                 ImGuiCond_Always);
        if (!ImGui::BeginPopup("device-finder"))
            return std::nullopt;
        ImGui::TextUnformatted(tr.translate("Find devices").c_str());
        ImGui::TextWrapped("%s", tr.translate(
            "Search name, address, MAC, external ID, #ID or type:router. Combine terms to narrow results.")
                .c_str());
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        const bool changed = ImGui::InputTextWithHint("##device-query",
            tr.translate("Search devices").c_str(), query_.data(), query_.size());
        const auto results = findDevices(topology, query_.data());
        auto current = std::find(results.nodes.begin(), results.nodes.end(), highlighted_);
        if (changed || current == results.nodes.end()) {
            highlighted_ = results.nodes.empty() ? -1 : results.nodes.front();
            current = results.nodes.begin();
        }
        bool moved = false;
        if (!results.nodes.empty()) {
            auto index = static_cast<int>(current - results.nodes.begin());
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                index = std::min(index + 1, static_cast<int>(results.nodes.size()) - 1);
                moved = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                index = std::max(index - 1, 0);
                moved = true;
            }
            highlighted_ = results.nodes[index];
        }
        ImGui::Text("%s: %zu | %s: %zu", tr.translate("Matches").c_str(), results.total,
                    tr.translate("Shown").c_str(), results.nodes.size());
        if (results.total > results.nodes.size())
            ImGui::TextWrapped("%s", tr.translate(
                "Showing the first 100 matches. Refine the search to find other devices.").c_str());
        std::optional<int> selected;
        if (highlighted_ >= 0 && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
            selected = highlighted_;
        if (ImGui::BeginTable("device-results", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            {0, std::max(1.0f, ImGui::GetContentRegionAvail().y - 24)})) {
            ImGui::TableSetupColumn(tr.translate("Device").c_str());
            ImGui::TableSetupColumn(tr.translate("Type").c_str());
            ImGui::TableSetupColumn(tr.translate("Addresses").c_str());
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (int id : results.nodes) {
                const auto& node = *topology.getNode(id);
                const auto& info = node.getDeviceInfo();
                ImGui::PushID(id);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const std::string label = "#" + std::to_string(id) + " " + node.getLabel();
                const auto position = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##result", highlighted_ == id,
                                      ImGuiSelectableFlags_SpanAllColumns,
                                      {0, ImGui::GetTextLineHeight()}))
                    selected = id;
                ImGui::GetWindowDrawList()->AddText(position, ImGui::GetColorU32(ImGuiCol_Text),
                                                   label.c_str());
                if (moved && highlighted_ == id)
                    ImGui::SetScrollHereY(0.5f);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(tr.translate(deviceDisplayName(info.type)).c_str());
                ImGui::TableNextColumn();
                if (!info.addresses.empty())
                    ImGui::TextUnformatted(info.addresses.front().c_str());
                if (ImGui::IsItemHovered() && info.addresses.size() > 1) {
                    ImGui::BeginTooltip();
                    for (const auto& address : info.addresses)
                        ImGui::TextUnformatted(address.c_str());
                    ImGui::EndTooltip();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("%s", tr.translate("Up/Down: navigate. Enter: focus. Esc: close.").c_str());
        if (selected)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return selected;
    }
}
