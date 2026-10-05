#include "gui/include/NetworkServicesPanel.hpp"
#include "gui/include/TranslationService.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/services/DeviceCLI.hpp"
#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace gui {
void NetworkServicesPanel::reset() { *this = NetworkServicesPanel{}; }

bool NetworkServicesPanel::render(kns::SimulationEngine& engine, int device, TranslationService& translations) {
    bool scheduled = false;
    const auto label = [&](const char* text) { return translations.label(text, text); };
    if (!ImGui::Begin(translations.label("Device Services", "device-services-window").c_str())) {
        ImGui::End();
        return false;
    }
    const auto* node = engine.getTopology().getNode(device);
    if (!node || !node->isActive()) {
        ImGui::TextWrapped("%s", translations.translate("Select a device on the topology canvas to configure HTTP and DNS services.").c_str());
        ImGui::End();
        return false;
    }
    if (device_ != device) {
        device_ = device;
        selected_.clear();
        editing_service_.clear();
        status_.clear();
        command_[0] = '\0';
        scroll_terminal_ = true;
    }
    ImGui::Text("Device %d - %s", device, node->getLabel().c_str());
    const auto type = node->getDeviceInfo().type;
    const auto caps = kns::deviceCapabilities(type);
    const bool hosts_services = caps.http_server || caps.dns_server;
    ImGui::TextWrapped("%s", translations.translate(kns::deviceRoleDescription(type)).c_str());
    if (hosts_services && !kns::canHostService(type, kind_ == 0 ? kns::ServiceKind::Http : kns::ServiceKind::Dns)) {
        kind_ = caps.http_server ? 0 : 1;
        port_ = kind_ == 0 ? 80 : 53;
    }
    const auto mutate = [&](auto operation) {
        try {
            auto services = engine.getTopology().getNode(device)->getServices();
            operation(services);
            engine.getTopology().setNodeServices(device, std::move(services));
            status_ = translations.translate("Service configuration updated");
        } catch (const std::exception& error) { status_ = error.what(); }
    };
    if (ImGui::BeginTabBar("service-tabs")) {
        // Keep draft values current before adjacent Apply/Send buttons read them.
        ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
        if (ImGui::BeginTabItem(label("Services").c_str())) {
            if (!hosts_services) ImGui::TextWrapped("%s", translations.translate("This device role cannot host application services.").c_str());
            ImGui::BeginDisabled(!hosts_services);
            ImGui::InputText(label("Name").c_str(), name_, sizeof(name_));
            if (ImGui::BeginCombo(label("Protocol").c_str(), kind_ == 0 ? "HTTP" : "DNS")) {
                for (int candidate = 0; candidate < 2; ++candidate) {
                    if (!kns::canHostService(type, candidate == 0 ? kns::ServiceKind::Http : kns::ServiceKind::Dns)) continue;
                    if (ImGui::Selectable(candidate == 0 ? "HTTP" : "DNS", kind_ == candidate)) {
                        kind_ = candidate;
                        port_ = kind_ == 0 ? 80 : 53;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::InputInt(label("Port").c_str(), &port_);
            if (ImGui::Button(label("Add service").c_str())) {
                mutate([&](auto& services) {
                    kns::NetworkService service;
                    service.name = name_;
                    service.kind = kind_ == 0 ? kns::ServiceKind::Http : kns::ServiceKind::Dns;
                    service.port = port_;
                    services.push_back(std::move(service));
                });
            }
            ImGui::Separator();
            // Copy: immediate GUI writes must not invalidate the displayed list.
            const auto services = engine.getTopology().getNode(device)->getServices();
            for (const auto& service : services) {
                const auto title = service.name + " (" + kns::serviceKindName(service.kind) + ':' + std::to_string(service.port) + ')';
                if (ImGui::Selectable(title.c_str(), selected_ == service.name)) {
                    selected_ = service.name;
                }
            }
            const auto selected = std::find_if(services.begin(), services.end(), [&](const auto& service) { return service.name == selected_; });
            if (selected != services.end()) {
                const auto revision = engine.getTopology().getNode(device)->getServiceRevision(selected_);
                const bool new_selection = editing_service_ != selected_;
                if (new_selection || editing_revision_ != revision) {
                    editing_service_ = selected_;
                    editing_revision_ = revision;
                    editing_port_ = selected->port;
                    editing_delay_ = selected->delay_ms;
                    if (new_selection) {
                        std::snprintf(key_, sizeof(key_), "%s", selected->kind == kns::ServiceKind::Http
                            ? (selected->pages.empty() ? "/" : selected->pages.begin()->first.c_str())
                            : (selected->records.empty() ? "server.example" : selected->records.begin()->first.c_str()));
                        std::snprintf(body_, sizeof(body_), "%s", "Hello from KNS");
                        std::snprintf(address_, sizeof(address_), "%s", "192.0.2.1");
                        http_status_ = 200;
                    }
                    if (const auto page = selected->pages.find(key_); page != selected->pages.end()) {
                        std::snprintf(body_, sizeof(body_), "%s", page->second.body.c_str());
                        http_status_ = page->second.status;
                    }
                    if (const auto record = selected->records.find(key_); record != selected->records.end())
                        std::snprintf(address_, sizeof(address_), "%s", record->second.c_str());
                }
                const auto update = [&](auto operation) {
                    mutate([&](auto& entries) {
                        auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& service) { return service.name == selected_; });
                        if (found != entries.end()) operation(*found);
                    });
                };
                bool enabled = selected->enabled;
                if (ImGui::Checkbox(label("Running").c_str(), &enabled)) update([&](auto& service) { service.enabled = enabled; });
                ImGui::InputInt(label("Listen port").c_str(), &editing_port_, 0, 0);
                bool apply = ImGui::IsItemDeactivatedAfterEdit() && ImGui::IsKeyPressed(ImGuiKey_Enter);
                ImGui::InputDouble(label("Processing delay (ms)").c_str(), &editing_delay_, 0, 0, "%.1f");
                apply |= ImGui::IsItemDeactivatedAfterEdit() && ImGui::IsKeyPressed(ImGuiKey_Enter);
                apply |= ImGui::Button(label("Apply settings").c_str());
                if (apply) update([&](auto& service) { service.port = editing_port_; service.delay_ms = editing_delay_; });
                for (const auto& [path, page] : selected->pages) {
                    if (ImGui::Selectable((path + " - " + std::to_string(page.status)).c_str())) {
                        std::snprintf(key_, sizeof(key_), "%s", path.c_str());
                        std::snprintf(body_, sizeof(body_), "%s", page.body.c_str());
                        http_status_ = page.status;
                    }
                }
                for (const auto& [name, address] : selected->records) {
                    if (ImGui::Selectable((name + " A " + address).c_str())) {
                        std::snprintf(key_, sizeof(key_), "%s", name.c_str());
                        std::snprintf(address_, sizeof(address_), "%s", address.c_str());
                    }
                }
                ImGui::InputText(label(selected->kind == kns::ServiceKind::Http ? "Path" : "Hostname").c_str(), key_, sizeof(key_));
                if (selected->kind == kns::ServiceKind::Http) {
                    ImGui::InputInt(label("HTTP status").c_str(), &http_status_);
                    ImGui::InputTextMultiline(label("Response body").c_str(), body_, sizeof(body_), ImVec2(-1, 100));
                } else ImGui::InputText(label("IPv4 address").c_str(), address_, sizeof(address_));
                if (ImGui::Button(label("Save entry").c_str())) update([&](auto& service) {
                    if (service.kind == kns::ServiceKind::Http) service.pages[key_] = {http_status_, body_};
                    else service.records[kns::normalizeDnsName(key_)] = address_;
                });
                ImGui::SameLine();
                if (ImGui::Button(label("Remove entry").c_str())) update([&](auto& service) {
                    const auto removed = service.kind == kns::ServiceKind::Http ? service.pages.erase(key_)
                        : service.records.erase(kns::normalizeDnsName(key_));
                    if (!removed) throw std::invalid_argument("Entry not found");
                });
                if (ImGui::Button(label("Remove service").c_str())) mutate([&](auto& entries) {
                    std::erase_if(entries, [&](const auto& service) { return service.name == selected_; });
                    selected_.clear();
                    editing_service_.clear();
                });
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(label("Client").c_str())) {
            if (!caps.service_client) ImGui::TextWrapped("%s", translations.translate("This device role cannot originate HTTP/DNS requests.").c_str());
            ImGui::BeginDisabled(!caps.service_client);
            ImGui::InputInt(label("Destination device").c_str(), &destination_);
            if (ImGui::Combo(label("Request protocol").c_str(), &request_kind_, "HTTP GET\0DNS A\0")) {
                request_port_ = request_kind_ == 0 ? 80 : 53;
                std::snprintf(query_, sizeof(query_), "%s", request_kind_ == 0 ? "/" : "server.example");
            }
            ImGui::InputInt(label("Destination port").c_str(), &request_port_);
            ImGui::InputText(label("Path or hostname").c_str(), query_, sizeof(query_));
            if (ImGui::Button(label("Send request").c_str())) {
                try {
                    const auto id = engine.networkServices().request(engine, device, destination_,
                        request_kind_ == 0 ? kns::ServiceKind::Http : kns::ServiceKind::Dns, request_port_, query_);
                    status_ = "Request #" + std::to_string(id) + " queued. Use Resume or Step.";
                    scheduled = true;
                } catch (const std::exception& error) { status_ = error.what(); }
            }
            ImGui::EndDisabled();
            ImGui::Separator();
            for (const auto& [id, request] : engine.networkServices().requests()) {
                if (request.source != device) continue;
                ImGui::Text("#%llu %s -> %d:%d %s", static_cast<unsigned long long>(id), kns::serviceKindName(request.kind),
                    request.destination, request.port, kns::serviceRequestStateName(request.state));
                if (request.state != kns::ServiceRequestState::Pending) {
                    ImGui::Text("Status: %d | %.3f ms", request.status, (request.finished_at - request.started_at) * 1000);
                    ImGui::TextWrapped("%s", request.response.c_str());
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(label("CLI").c_str())) {
            ImGui::TextDisabled("%s", translations.translate("Enter help for device commands. Use Resume/Step to advance requests.").c_str());
            auto& history = terminal_[device];
            ImGui::BeginChild("terminal-output", ImVec2(0, 200), ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(history.c_str());
            if (scroll_terminal_) { ImGui::SetScrollHereY(1.0f); scroll_terminal_ = false; }
            ImGui::EndChild();
            const bool enter = ImGui::InputText(label("Command").c_str(), command_, sizeof(command_), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button(label("Execute").c_str()) || enter) {
                const auto result = kns::DeviceCLI::execute(engine, device, command_);
                history += "device-" + std::to_string(device) + "> " + command_ + '\n' + result.output;
                if (history.size() > 32768) {
                    const auto newline = history.find('\n', history.size() - 32768);
                    history.erase(0, newline == std::string::npos ? history.size() : newline + 1);
                }
                scroll_terminal_ = true;
                scheduled |= result.scheduled;
                command_[0] = '\0';
            }
            ImGui::EndTabItem();
        }
        ImGui::PopItemFlag();
        ImGui::EndTabBar();
    }
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    ImGui::End();
    return scheduled;
}
} // namespace gui
