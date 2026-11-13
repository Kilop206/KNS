#include "traffic/TrafficRun.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/utils/PacketUtils.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace kns::traffic {
namespace {
using Json = nlohmann::json;
double number(const Json& value, double low, double high, const char* label) {
    if (!value.is_number() || !std::isfinite(value.get<double>()) || value < low || value > high)
        throw std::invalid_argument(std::string(label) + " must be finite and within bounds");
    return value.get<double>();
}
std::uint64_t integer(const Json& value, std::uint64_t low, std::uint64_t high, const char* label) {
    if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>() < 0) ||
        value.get<std::uint64_t>() < low || value.get<std::uint64_t>() > high)
        throw std::invalid_argument(std::string(label) + " must be an integer within bounds");
    return value.get<std::uint64_t>();
}
void fields(const Json& value, const std::set<std::string>& required, const std::set<std::string>& optional = {}) {
    if (!value.is_object()) throw std::invalid_argument("Traffic configuration must be an object");
    for (const auto& key : required) if (!value.contains(key)) throw std::invalid_argument("Missing traffic field: " + key);
    for (const auto& item : value.items())
        if (!required.contains(item.key()) && !optional.contains(item.key())) throw std::invalid_argument("Unknown traffic field: " + item.key());
}
std::uint64_t streamSeed(std::uint64_t seed, std::uint32_t id) {
    // SplitMix64 finalizer: fixed integer arithmetic, no process-dependent hash.
    auto x = seed + 0x9e3779b97f4a7c15ULL * id;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
struct Counters {
    std::uint64_t generated = 0, delivered = 0, dropped = 0, delivered_bytes = 0, on_time = 0, late = 0;
};
class Outcome final : public PacketOutcome {
    std::shared_ptr<Counters> counters_;
    bool finished_ = false;
public:
    explicit Outcome(std::shared_ptr<Counters> counters) : counters_(std::move(counters)) {}
    void rejected() noexcept override { if (!finished_) { finished_ = true; ++counters_->dropped; } }
    void delivered(SimulationEngine& engine, const Packet& packet) override {
        if (finished_) return;
        finished_ = true; ++counters_->delivered;
        counters_->delivered_bytes += static_cast<std::uint64_t>(packet.serializedSize());
        if (packet.workload->deadline_at_s) {
            if (engine.now() <= *packet.workload->deadline_at_s) ++counters_->on_time;
            else ++counters_->late;
        }
    }
};
struct Flow {
    Json config;
    std::uint64_t seed;
    std::mt19937_64 random;
    std::shared_ptr<Counters> counters = std::make_shared<Counters>();
    bool generation_pending = false;
    Flow(Json input, std::uint64_t root_seed) : config(std::move(input)),
        seed(streamSeed(root_seed, config.at("id"))), random(seed) {}
    double uniform() { return static_cast<double>(random() >> 11) * 0x1.0p-53; }
    std::optional<double> next(double previous) {
        const auto& schedule = config.at("schedule");
        const auto kind = schedule.at("kind").get<std::string>();
        const auto ordinal = counters->generated;
        const double start = config.at("start_s"), end = config.at("end_s");
        double at = start;
        if (kind == "periodic") at += ordinal * schedule.at("interval_s").get<double>();
        else if (kind == "cbr") at += ordinal * (8.0 * config.at("wire_bytes").get<double>() / schedule.at("rate_bps").get<double>());
        else if (kind == "custom") {
            const auto& offsets = schedule.at("offsets_s");
            if (ordinal >= offsets.size()) return {};
            at += offsets.at(ordinal).get<double>();
        } else if (kind == "burst") {
            const auto n = schedule.at("burst_packets").get<std::uint64_t>();
            at += (ordinal / n) * schedule.at("burst_interval_s").get<double>() +
                (ordinal % n) * schedule.at("packet_interval_s").get<double>();
        } else {
            double delay;
            if (kind == "poisson") delay = -std::log1p(-std::max(uniform(), 0x1.0p-53)) / schedule.at("rate_pps").get<double>();
            else {
                const double low = schedule.at("min_interval_s"), high = schedule.at("max_interval_s");
                delay = low + (high - low) * uniform();
            }
            at = previous + delay;
            if (at <= previous) at = std::nextafter(previous, std::numeric_limits<double>::infinity());
        }
        return at < end ? std::optional<double>(at) : std::nullopt;
    }
};
class GenerateEvent final : public Event {
    std::shared_ptr<TrafficRun> run_;
    std::size_t flow_;
public:
    GenerateEvent(double at, std::shared_ptr<TrafficRun> run, std::size_t flow) : Event(at), run_(std::move(run)), flow_(flow) {}
    void execute(SimulationEngine& engine) override { run_->generate(engine, flow_); }
};
void validate(const Json& plan, const SimulationEngine& engine) {
    fields(plan, {"schema_version", "seed", "max_packets", "flows"});
    if (plan.at("schema_version") != "1.0") throw std::invalid_argument("Unsupported traffic schema_version");
    integer(plan.at("seed"), 0, std::numeric_limits<std::uint64_t>::max(), "seed");
    integer(plan.at("max_packets"), 1, 1000000, "max_packets");
    const auto& flows = plan.at("flows");
    if (!flows.is_array() || flows.empty() || flows.size() > 256) throw std::invalid_argument("Traffic requires 1..256 flows");
    std::set<std::uint64_t> ids;
    for (const auto& flow : flows) {
        fields(flow, {"id", "source", "destination", "start_s", "end_s", "wire_bytes", "schedule"}, {"priority", "ttl_hops", "deadline_s"});
        if (!ids.insert(integer(flow.at("id"), 1, 0xffffffff, "flow id")).second) throw std::invalid_argument("Duplicate traffic flow id");
        for (const auto* key : {"source", "destination"}) {
            const auto node = integer(flow.at(key), 0, std::numeric_limits<int>::max(), key);
            if (node >= static_cast<std::uint64_t>(engine.getTopology().size())) throw std::invalid_argument("Traffic endpoint does not exist");
        }
        if (flow.at("source") == flow.at("destination")) throw std::invalid_argument("Traffic endpoints must differ");
        const double start = number(flow.at("start_s"), 0, 1e6, "start_s");
        const double end = number(flow.at("end_s"), 0, 1e6, "end_s");
        if (start >= end) throw std::invalid_argument("Traffic end_s must be after start_s");
        const auto size = integer(flow.at("wire_bytes"), 1, 1048576, "wire_bytes");
        integer(flow.value("priority", Json(0)), 0, 7, "priority");
        integer(flow.value("ttl_hops", Json(64)), 1, 4096, "ttl_hops");
        if (flow.contains("deadline_s")) number(flow.at("deadline_s"), 1e-9, 1e6, "deadline_s");
        const auto& schedule = flow.at("schedule");
        if (!schedule.is_object() || !schedule.contains("kind") || !schedule.at("kind").is_string()) throw std::invalid_argument("Missing traffic schedule kind");
        const auto kind = schedule.at("kind").get<std::string>();
        if (kind == "periodic") {
            fields(schedule, {"kind", "interval_s"}); number(schedule.at("interval_s"), 1e-9, 1e6, "interval_s");
        } else if (kind == "cbr" || kind == "poisson") {
            const char* key = kind == "cbr" ? "rate_bps" : "rate_pps";
            fields(schedule, {"kind", key}); const double rate = number(schedule.at(key), 1e-9, 1e15, key);
            if (kind == "cbr" && 8.0 * size / rate < 1e-9) throw std::invalid_argument("CBR interval must be at least 1 ns");
        } else if (kind == "uniform") {
            fields(schedule, {"kind", "min_interval_s", "max_interval_s"});
            const double low = number(schedule.at("min_interval_s"), 1e-9, 1e6, "min_interval_s");
            if (number(schedule.at("max_interval_s"), 1e-9, 1e6, "max_interval_s") < low) throw std::invalid_argument("Uniform intervals are reversed");
        } else if (kind == "burst") {
            fields(schedule, {"kind", "burst_packets", "burst_interval_s", "packet_interval_s"});
            const auto n = integer(schedule.at("burst_packets"), 1, 1000000, "burst_packets");
            const double between = number(schedule.at("burst_interval_s"), 1e-9, 1e6, "burst_interval_s");
            if ((n - 1) * number(schedule.at("packet_interval_s"), 0, 1e6, "packet_interval_s") >= between) throw std::invalid_argument("Bursts must not overlap");
        } else if (kind == "custom") {
            fields(schedule, {"kind", "offsets_s"});
            const auto& offsets = schedule.at("offsets_s");
            if (!offsets.is_array() || offsets.empty() || offsets.size() > 10000) throw std::invalid_argument("Custom traffic needs 1..10000 offsets");
            double previous = -1;
            for (const auto& item : offsets) {
                const auto offset = number(item, 0, end - start, "custom offset");
                if (offset < previous || start + offset >= end) throw std::invalid_argument("Custom offsets must be ordered and before end_s");
                previous = offset;
            }
        } else throw std::invalid_argument("Unknown traffic schedule kind");
    }
}
}
struct TrafficRun::Data {
    Json config;
    std::vector<Flow> flows;
    std::uint64_t generated = 0, maximum;
    bool limited = false;
    explicit Data(const Json& plan) : config(plan), maximum(plan.at("max_packets")) {
        for (const auto& flow : plan.at("flows")) flows.emplace_back(flow, plan.at("seed"));
    }
};
TrafficRun::TrafficRun(const Json& plan) : data_(std::make_unique<Data>(plan)) {}
TrafficRun::~TrafficRun() = default;
std::shared_ptr<TrafficRun> TrafficRun::schedule(SimulationEngine& engine, const Json& plan) {
    validate(plan, engine);
    auto run = std::shared_ptr<TrafficRun>(new TrafficRun(plan));
    engine.claimTrafficPlan();
    for (std::size_t i = 0; i < run->data_->flows.size(); ++i) {
        auto& flow = run->data_->flows[i];
        if (const auto at = flow.next(flow.config.at("start_s"))) {
            engine.schedule(std::make_unique<GenerateEvent>(*at, run, i));
            flow.generation_pending = true;
        }
    }
    return run;
}
void TrafficRun::generate(SimulationEngine& engine, std::size_t index) {
    auto& d = *data_;
    auto& flow = d.flows.at(index);
    flow.generation_pending = false;
    if (d.generated >= d.maximum) { d.limited = true; return; }
    auto& counters = *flow.counters;
    const auto& config = flow.config;
    Packet packet(config.at("source"), config.at("destination"), config.at("source"), engine.now(), config.at("wire_bytes"), 0);
    packet.workload = PacketWorkload{config.at("id"), counters.generated, config.value("priority", std::uint8_t{0}),
        config.value("ttl_hops", std::uint32_t{64}), {}, std::make_shared<Outcome>(flow.counters)};
    if (config.contains("deadline_s")) packet.workload->deadline_at_s = engine.now() + config.at("deadline_s").get<double>();
    ++counters.generated; ++d.generated;
    if (!PacketUtils::sendPacketThroughTopology(engine, packet)) packet.workload->outcome->rejected();
    if (const auto next = flow.next(engine.now())) {
        if (d.generated >= d.maximum) d.limited = true;
        else {
            engine.schedule(std::make_unique<GenerateEvent>(*next, shared_from_this(), index));
            flow.generation_pending = true;
        }
    }
}
Json TrafficRun::toJson(double now) const {
    const auto& d = *data_;
    Json flows = Json::array();
    std::uint64_t delivered = 0, dropped = 0;
    bool generation_pending = false;
    for (const auto& flow : d.flows) {
        const auto& c = *flow.counters;
        delivered += c.delivered; dropped += c.dropped;
        generation_pending = generation_pending || flow.generation_pending;
        flows.push_back({{"id", flow.config.at("id")}, {"stream_seed", flow.seed}, {"config", flow.config},
            {"generated", c.generated}, {"delivered", c.delivered}, {"dropped", c.dropped},
            {"pending", c.generated - c.delivered - c.dropped}, {"delivered_wire_bytes", c.delivered_bytes},
            {"delivered_on_time", c.on_time}, {"delivered_late", c.late}, {"generation_pending", flow.generation_pending}});
    }
    return {{"schema_version", "1.0"}, {"config", d.config}, {"simulation_time_s", now},
        {"generation_limit_reached", d.limited}, {"generation_pending", generation_pending},
        {"generated", d.generated}, {"delivered", delivered}, {"dropped", dropped},
        {"pending", d.generated - delivered - dropped}, {"flows", flows},
        {"model", "abstract one-way wire packets; FIFO/RED link policies; priority is metadata; deadlines measured at destination; no TCP/application protocol or real sockets"}};
}
}
