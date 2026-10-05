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
        status_.clear();
        command_[0] = '\0';
    }
    ImGui::Text("Device %d - %s", device, node->getLabel().c_str());
    const auto mutate = [&](auto operation) {
        try {
            auto services = engine.getTopology().getNode(device)->getServices();
            operation(services);
            engine.getTopology().setNodeServices(device, std::move(services));
            status_ = translations.translate("Service configuration updated");
        } catch (const std::exception& error) { status_ = error.what(); }
    };
    if (ImGui::BeginTabBar("service-tabs")) {
        if (ImGui::BeginTabItem(label("Services").c_str())) {
            ImGui::InputText(label("Name").c_str(), name_, sizeof(name_));
            if (ImGui::Combo(label("Protocol").c_str(), &kind_, "HTTP\0DNS\0")) port_ = kind_ == 0 ? 80 : 53;
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
                    std::snprintf(key_, sizeof(key_), "%s", service.kind == kns::ServiceKind::Http ? "/" : "server.example");
                }
            }
            const auto selected = std::find_if(services.begin(), services.end(), [&](const auto& service) { return service.name == selected_; });
            if (selected != services.end()) {
                const auto update = [&](auto operation) {
                    mutate([&](auto& entries) {
                        auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& service) { return service.name == selected_; });
                        if (found != entries.end()) operation(*found);
                    });
                };
                bool enabled = selected->enabled;
                if (ImGui::Checkbox(label("Running").c_str(), &enabled)) update([&](auto& service) { service.enabled = enabled; });
                int port = selected->port;
                if (ImGui::InputInt(label("Listen port").c_str(), &port, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue))
                    update([&](auto& service) { service.port = port; });
                double delay = selected->delay_ms;
                if (ImGui::InputDouble(label("Processing delay (ms)").c_str(), &delay, 0, 0, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue))
                    update([&](auto& service) { service.delay_ms = delay; });
                ImGui::TextDisabled("%s", translations.translate("Press Enter to apply port or delay.").c_str());
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
                    if (service.kind == kns::ServiceKind::Http) service.pages.erase(key_);
                    else service.records.erase(kns::normalizeDnsName(key_));
                });
                if (ImGui::Button(label("Remove service").c_str())) mutate([&](auto& entries) {
                    std::erase_if(entries, [&](const auto& service) { return service.name == selected_; });
                });
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(label("Client").c_str())) {
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
            ImGui::EndChild();
            const bool enter = ImGui::InputText(label("Command").c_str(), command_, sizeof(command_), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button(label("Execute").c_str()) || enter) {
                const auto result = kns::DeviceCLI::execute(engine, device, command_);
                history += "device-" + std::to_string(device) + "> " + command_ + '\n' + result.output;
                if (history.size() > 32768) history.erase(0, history.size() - 32768);
                scheduled |= result.scheduled;
                command_[0] = '\0';
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
    ImGui::End();
    return scheduled;
}
} // namespace gui
