#include "network/services/DeviceCLI.hpp"
#include "engine/core/SimulationEngine.hpp"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace kns {
namespace {
std::vector<std::string> tokenize(const std::string& command) {
    if (command.size() > 8192) throw std::invalid_argument("Command exceeds 8192 bytes");
    std::istringstream input(command);
    std::vector<std::string> words;
    while (input >> std::ws && !input.eof()) {
        std::string word;
        if (!(input >> std::quoted(word))) throw std::invalid_argument("Unterminated quoted argument");
        words.push_back(std::move(word));
    }
    return words;
}
int integer(const std::string& text) {
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::invalid_argument("Expected an integer: " + text);
    return value;
}
void count(const std::vector<std::string>& words, std::size_t size) {
    if (words.size() != size) throw std::invalid_argument("Wrong argument count; enter help for syntax");
}
}

std::string DeviceCLI::help(DeviceType type) {
    const auto caps = deviceCapabilities(type);
    std::string result = std::string(deviceRoleDescription(type)) + "\nhelp\nshow role\nshow interfaces\nshow routes\nshow services\nshow requests\n";
    if (caps.http_server || caps.dns_server) {
        result += std::string("service add ") + (caps.http_server && caps.dns_server ? "<http|dns>" : caps.http_server ? "http" : "dns") + " <name> <port>\n";
        result += "service remove <name>\nservice start <name>\nservice stop <name>\nservice delay <name> <milliseconds>\n";
    }
    if (caps.http_server) result += "service page <name> <path> <status> \"body\"\nservice unpage <name> <path>\n";
    if (caps.dns_server) result += "service record <name> <hostname> <IPv4>\nservice unrecord <name> <hostname>\n";
    if (caps.service_client) result += "http get <device-id> <port> <path>\ndns query <device-id> <port> <hostname>\n"
        "Requests run on simulated time: use Resume/Step in the GUI, or run in the stdin CLI.\n";
    return result;
}

DeviceCommandResult DeviceCLI::execute(SimulationEngine& engine, int device, const std::string& command) {
    try {
        const auto words = tokenize(command);
        if (words.empty()) return {true, false, {}};
        const auto* node = engine.getTopology().getNode(device);
        if (!node || !node->isActive()) throw std::invalid_argument("Select an active device");
        if (words[0] == "help") { count(words, 1); return {true, false, help(node->getDeviceInfo().type)}; }
        if (words[0] == "show") {
            count(words, 2);
            std::ostringstream output;
            if (words[1] == "role") {
                output << toString(node->getDeviceInfo().type) << ": " << deviceRoleDescription(node->getDeviceInfo().type) << '\n';
            } else if (words[1] == "interfaces") {
                for (const auto& link : engine.getTopology().getLinksFromNode(device))
                    output << "link " << link->getId() << " peer=" << link->getOtherNode(device)
                        << (link->isUp() ? " up" : " down") << " bandwidth=" << link->getBandwidthMbps() << "Mbps\n";
            } else if (words[1] == "routes") {
                for (const auto& route : engine.getRoutingTable(device)) {
                    if (route.next_hop < 0) continue;
                    output << "destination=" << route.destination << " via=" << route.next_hop << " link=" << *route.link_id << '\n';
                }
            } else if (words[1] == "services") {
                for (const auto& service : node->getServices()) {
                    output << service.name << ' ' << serviceKindName(service.kind) << ':' << service.port
                        << (service.enabled ? " running" : " stopped") << " delay=" << service.delay_ms << "ms\n";
                    for (const auto& [path, page] : service.pages)
                        output << "  " << path << ' ' << page.status << ' ' << std::quoted(page.body) << '\n';
                    for (const auto& [name, address] : service.records)
                        output << "  " << name << " A " << address << '\n';
                }
                if (node->getServices().empty()) output << "No services configured\n";
            } else if (words[1] == "requests") {
                for (const auto& [id, request] : engine.networkServices().requests()) {
                    if (request.source != device) continue;
                    output << '#' << id << ' ' << serviceKindName(request.kind) << " device=" << request.destination
                        << ':' << request.port << ' ' << serviceRequestStateName(request.state);
                    if (request.state != ServiceRequestState::Pending)
                        output << " status=" << request.status << " elapsed=" << (request.finished_at - request.started_at) * 1000
                            << "ms " << std::quoted(request.response);
                    output << '\n';
                }
            } else throw std::invalid_argument("Use show role, interfaces, routes, services or requests");
            return {true, false, output.str()};
        }
        if (words[0] == "http" || words[0] == "dns") {
            count(words, 5);
            if ((words[0] == "http" && words[1] != "get") || (words[0] == "dns" && words[1] != "query"))
                throw std::invalid_argument("Use http get or dns query");
            const auto id = engine.networkServices().request(engine, device, integer(words[2]),
                parseServiceKind(words[0]), integer(words[3]), words[4]);
            return {true, true, "Request #" + std::to_string(id) + " queued; advance simulation to receive the result\n"};
        }
        if (words[0] != "service" || words.size() < 3) throw std::invalid_argument("Unknown command; enter help");
        const auto caps = deviceCapabilities(node->getDeviceInfo().type);
        if (!caps.http_server && !caps.dns_server) throw std::invalid_argument("This device role cannot host services; use show role");
        auto services = node->getServices();
        if (words[1] == "add") {
            count(words, 5);
            NetworkService service;
            service.kind = parseServiceKind(words[2]);
            service.name = words[3];
            service.port = integer(words[4]);
            services.push_back(std::move(service));
        } else {
            const auto found = std::find_if(services.begin(), services.end(), [&](const auto& service) { return service.name == words[2]; });
            if (found == services.end()) throw std::invalid_argument("Service not found: " + words[2]);
            auto& service = *found;
            if (words[1] == "remove") { count(words, 3); services.erase(found); }
            else if (words[1] == "start" || words[1] == "stop") { count(words, 3); service.enabled = words[1] == "start"; }
            else if (words[1] == "delay") { count(words, 4); service.delay_ms = integer(words[3]); }
            else if (words[1] == "page") {
                count(words, 6);
                if (service.kind != ServiceKind::Http) throw std::invalid_argument("Pages require an HTTP service");
                service.pages[words[3]] = {integer(words[4]), words[5]};
            } else if (words[1] == "unpage") {
                count(words, 4);
                if (!service.pages.erase(words[3])) throw std::invalid_argument("Page not found");
            } else if (words[1] == "record") {
                count(words, 5);
                if (service.kind != ServiceKind::Dns) throw std::invalid_argument("Records require a DNS service");
                service.records[normalizeDnsName(words[3])] = words[4];
            } else if (words[1] == "unrecord") {
                count(words, 4);
                if (!service.records.erase(normalizeDnsName(words[3]))) throw std::invalid_argument("Record not found");
            } else throw std::invalid_argument("Unknown service command; enter help");
        }
        engine.getTopology().setNodeServices(device, std::move(services));
        return {true, false, "Service configuration updated\n"};
    } catch (const std::exception& error) {
        return {false, false, std::string("Error: ") + error.what() + '\n'};
    }
}
} // namespace kns
