#include "engine/experiments/FaultLab.hpp"
#include "engine/core/SimulationEngine.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>

namespace kns {
namespace {
    using Json = nlohmann::json;
    void fields(const Json& object, std::initializer_list<const char*> allowed) {
        if (!object.is_object()) throw std::invalid_argument("Fault plan entries must be objects");
        for (const auto& item : object.items()) {
            if (std::none_of(allowed.begin(), allowed.end(), [&](const char* key) { return item.key() == key; }))
                throw std::invalid_argument("Unknown fault plan field: " + item.key());
        }
    }
    double number(const Json& value, const char* name) {
        if (!value.is_number()) throw std::invalid_argument(std::string(name) + " must be numeric");
        const double result = value.get<double>();
        if (!std::isfinite(result) || result < 0) throw std::invalid_argument(std::string(name) + " must be finite and nonnegative");
        return result;
    }
    int index(const Json& value, int size, const char* name) {
        if (!value.is_number_integer() || value < 0 || value >= size)
            throw std::invalid_argument(std::string(name) + " is out of range");
        return value.get<int>();
    }
    class Action final : public Event {
        std::function<void(SimulationEngine&)> action_;
    public:
        Action(double at, std::function<void(SimulationEngine&)> action)
            : Event(at), action_(std::move(action)) {}
        void execute(SimulationEngine& engine) override { action_(engine); }
    };
    const char* status(RouteStatus value) {
        switch (value) {
            case RouteStatus::Reachable: return "reachable";
            case RouteStatus::Unreachable: return "unreachable";
            case RouteStatus::InvalidEndpoint: return "invalid_endpoint";
            case RouteStatus::ForwardingLoop: return "forwarding_loop";
        }
        return "unknown";
    }
}

std::shared_ptr<FaultReport> FaultLab::schedule(SimulationEngine& engine, const Json& plan) {
    fields(plan, {"schema_version", "in_flight_policy", "events", "probes", "observe_at"});
    if (plan.value("schema_version", "") != "1.0" ||
        plan.value("in_flight_policy", "") != "complete_accepted_hops")
        throw std::invalid_argument("Fault plan requires version 1.0 and complete_accepted_hops policy");
    if (!plan.contains("events") || !plan.at("events").is_array() || plan.at("events").size() > 10000)
        throw std::invalid_argument("Fault events must be an array of at most 10000 entries");
    if (!plan.contains("probes") || !plan.at("probes").is_array() || plan.at("probes").size() > 64)
        throw std::invalid_argument("Fault probes must be an array of at most 64 entries");
    if (!plan.contains("observe_at") || !plan.at("observe_at").is_array() || plan.at("observe_at").size() > 10000)
        throw std::invalid_argument("observe_at must be an array of at most 10000 timestamps");
    const auto links = engine.getTopology().getLinks();
    std::set<double> times{engine.now()};
    for (const auto& event : plan.at("events")) {
        fields(event, {"at", "type", "link_index", "value"});
        const double at = number(event.at("at"), "at");
        if (at < engine.now()) throw std::invalid_argument("Fault event is in the past");
        index(event.at("link_index"), static_cast<int>(links.size()), "link_index");
        const auto type = event.at("type").get<std::string>();
        if (type == "link_down" || type == "link_up") {
            if (event.contains("value")) throw std::invalid_argument("Availability events do not accept value");
        } else if (type == "bandwidth_mbps" || type == "delay_ms" || type == "loss_probability") {
            const double value = number(event.at("value"), "value");
            if ((type == "bandwidth_mbps" && value == 0) || (type == "loss_probability" && value > 1))
                throw std::invalid_argument("Fault parameter value is outside its domain");
        } else throw std::invalid_argument("Unsupported fault type: " + type);
        times.insert(at);
    }
    for (const auto& probe : plan.at("probes")) {
        fields(probe, {"source", "destination"});
        const int source = index(probe.at("source"), engine.getTopology().size(), "probe source");
        const int dest = index(probe.at("destination"), engine.getTopology().size(), "probe destination");
        if (source == dest || !engine.getTopology().getNode(source)->isActive() || !engine.getTopology().getNode(dest)->isActive())
            throw std::invalid_argument("Fault probes require distinct active endpoints");
    }
    for (const auto& time : plan.at("observe_at")) {
        const auto at = number(time, "observation time");
        if (at < engine.now()) throw std::invalid_argument("Observation is in the past");
        times.insert(at);
    }
    if (engine.executionState() == ExecutionState::Cancelled || engine.executionState() == ExecutionState::Failed)
        throw std::logic_error("Cannot schedule faults on a terminal engine");
    if (!links.empty() && times.size() > 1000000 / links.size())
        throw std::invalid_argument("Fault observations exceed one million link samples");
    auto report = std::make_shared<FaultReport>();
    report->plan = plan;
    std::vector<std::uint64_t> ids;
    for (const auto& link : links) ids.push_back(link->getId());
    // Construct all faults first: observations at a tie see the final planned state.
    for (const auto& event : plan.at("events")) {
        const auto target = links[event.at("link_index").get<std::size_t>()]->getId();
        engine.schedule(std::make_unique<Action>(event.at("at").get<double>(), [report, event, target](auto& e) {
            const auto& current = e.getTopology().getLinks();
            const auto found = std::find_if(current.begin(), current.end(), [target](const auto& link) { return link->getId() == target; });
            if (found == current.end()) throw std::runtime_error("Fault target link was removed before execution");
            auto& link = **found;
            const auto type = event.at("type").get<std::string>();
            Json previous;
            if (type == "link_down" || type == "link_up") { previous = link.isUp(); link.setUp(type == "link_up"); }
            else if (type == "bandwidth_mbps") { previous = link.getBandwidthMbps(); link.setBandwidthMbps(event.at("value")); }
            else if (type == "delay_ms") { previous = link.getDelayMs(); link.setDelayMs(event.at("value")); }
            else { previous = link.getLossProb(); link.setLossProb(event.at("value")); }
            report->changes.push_back({{"at", e.now()}, {"event", event}, {"previous", previous}, {"status", "applied"}});
        }));
    }
    for (const auto at : times) {
        engine.schedule(std::make_unique<Action>(at, [report, ids](auto& e) {
            const auto& s = e.getStats();
            Json sample{{"at", e.now()}, {"packets_sent", s.packets_sent}, {"packets_delivered", s.packets_delivered},
                {"packets_lost", s.packets_lost}, {"data_packets_delivered", s.data_packets_delivered},
                {"total_latency_s", s.total_latency}, {"packets_in_transit", e.getPacketsInTransit().size()},
                {"links", Json::array()}, {"probes", Json::array()}};
            for (const auto& link : e.getTopology().getLinks()) {
                const auto found = std::find(ids.begin(), ids.end(), link->getId());
                if (found != ids.end()) sample["links"].push_back({{"link_index", std::distance(ids.begin(), found)},
                    {"up", link->isUp()}, {"bandwidth_mbps", link->getBandwidthMbps()}, {"delay_ms", link->getDelayMs()},
                    {"loss_probability", link->getLossProb()}, {"queue_occupancy", link->getQueueSize()}});
            }
            for (const auto& probe : report->plan.at("probes")) {
                const auto route = e.traceRoute(probe.at("source"), probe.at("destination"));
                Json hops = Json::array();
                for (const auto& hop : route.hops) {
                    const auto found = std::find(ids.begin(), ids.end(), hop.link_id);
                    hops.push_back({{"from", hop.from}, {"to", hop.to},
                        {"link_index", found == ids.end() ? Json(nullptr) : Json(std::distance(ids.begin(), found))}});
                }
                sample["probes"].push_back({{"source", probe.at("source")}, {"destination", probe.at("destination")},
                    {"status", status(route.status)}, {"hops", hops}});
            }
            report->observations.push_back(std::move(sample));
        }));
    }
    return report;
}

nlohmann::json FaultReport::toJson() const {
    Json windows = Json::array();
    for (std::size_t i = 1; i < observations.size(); ++i) {
        const auto& a = observations[i - 1];
        const auto& b = observations[i];
        const double duration = b.at("at").get<double>() - a.at("at").get<double>();
        const int delivered = b.at("packets_delivered").get<int>() - a.at("packets_delivered").get<int>();
        const int data = b.at("data_packets_delivered").get<int>() - a.at("data_packets_delivered").get<int>();
        windows.push_back({{"from_s", a.at("at")}, {"to_s", b.at("at")},
            {"packets_sent", b.at("packets_sent").get<int>() - a.at("packets_sent").get<int>()},
            {"packets_delivered", delivered}, {"packets_lost", b.at("packets_lost").get<int>() - a.at("packets_lost").get<int>()},
            {"throughput_pps", duration > 0 ? Json(delivered / duration) : Json(nullptr)},
            {"mean_data_latency_s", data > 0 ? Json((b.at("total_latency_s").get<double>() - a.at("total_latency_s").get<double>()) / data) : Json(nullptr)}});
    }
    Json outages = Json::array();
    for (std::size_t probe = 0; probe < plan.at("probes").size(); ++probe) {
        std::optional<double> start;
        for (const auto& observation : observations) {
            const bool reachable = observation.at("probes").at(probe).at("status") == "reachable";
            const double at = observation.at("at");
            if (!reachable && !start) start = at;
            if (reachable && start) {
                outages.push_back({{"probe", plan.at("probes").at(probe)}, {"from_s", *start},
                    {"to_s", at}, {"duration_s", at - *start}, {"censored", false}});
                start.reset();
            }
        }
        if (start) outages.push_back({{"probe", plan.at("probes").at(probe)}, {"from_s", *start},
            {"to_s", nullptr}, {"duration_s", nullptr}, {"censored", true}});
    }
    Json linkOutages = Json::array();
    if (!observations.empty()) for (const auto& initial : observations.front().at("links")) {
        const auto linkIndex = initial.at("link_index");
        std::optional<double> start;
        for (const auto& observation : observations) {
            const auto& links = observation.at("links");
            const auto found = std::find_if(links.begin(), links.end(), [&](const auto& link) { return link.at("link_index") == linkIndex; });
            const bool up = found != links.end() && found->at("up") == true;
            const double at = observation.at("at");
            if (!up && !start) start = at;
            if (up && start) {
                linkOutages.push_back({{"link_index", linkIndex}, {"from_s", *start}, {"to_s", at},
                    {"duration_s", at - *start}, {"censored", false}});
                start.reset();
            }
        }
        if (start) linkOutages.push_back({{"link_index", linkIndex}, {"from_s", *start},
            {"to_s", nullptr}, {"duration_s", nullptr}, {"censored", true}});
    }
    return {{"schema_version", "1.0"}, {"in_flight_policy", "complete_accepted_hops"},
        {"plan", plan}, {"changes", changes}, {"observations", observations}, {"windows", windows},
        {"sampled_connectivity_outages", outages},
        {"sampled_link_outages", linkOutages},
        {"routing_model", "synchronous_recomputation"}, {"service_recovery_s", nullptr}};
}
}
