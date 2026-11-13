#include "analysis/MetricsCollector.hpp"
#include "analysis/LatencyStatistics.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <optional>
#include <stdexcept>

namespace kns::analysis {
namespace {
    using Json = nlohmann::json;
    class SampleEvent final : public Event {
        std::shared_ptr<MetricsCollector> collector_;
    public:
        SampleEvent(double time, std::shared_ptr<MetricsCollector> collector) : Event(time), collector_(std::move(collector)) {}
        void execute(SimulationEngine& engine) override { collector_->sample(engine); }
    };
    std::size_t limit(const Json& config, const char* name, int fallback, int maximum) {
        const auto value = config.value(name, Json(fallback));
        if (!value.is_number_integer() || value < 1 || value > maximum)
            throw std::invalid_argument(std::string(name) + " is outside its integer limit");
        return value.get<std::size_t>();
    }
    struct Group {
        std::uint64_t packets = 0, wire_bytes = 0;
        std::uint64_t tcp_payload_bytes = 0;
        LatencyStatistics latency;
        std::optional<LatencyStatistics> variation;
        std::optional<double> previous_data_latency;
        explicit Group(std::size_t retained, bool flow) : latency(retained) {
            if (flow) variation.emplace(retained);
        }
    };
    struct Reservation { double start, end; int direction; };
    std::string flowKey(const Packet& packet) {
        return std::to_string(packet.session_id) + ":" + std::to_string(packet.source) + ":" +
            std::to_string(packet.destination) + ":" + std::to_string(packet.tcp.source_port) + ":" +
            std::to_string(packet.tcp.destination_port);
    }
    struct LinkRecord {
        std::size_t index;
        LinkMode mode;
        bool complete = true;
        std::deque<Reservation> reservations;
        Json previous;
        double peak_utilization = 0, peak_queue = 0, util_from = 0, util_to = 0, queue_at = 0;
        std::uint64_t valid_utilization_windows = 0, queue_samples = 0;
        std::uint64_t pressure_windows = 0, pressure_streak = 0, longest_pressure_streak = 0;
        double pressure_start = 0, longest_pressure_from = 0, longest_pressure_to = 0;
        double valid_duration = 0, high_pressure_duration = 0;
    };
}
struct MetricsCollector::Data {
    Json config;
    double horizon, interval, previous_time = 0;
    std::size_t max_samples, max_groups, group_retention, max_reservations, reservation_count = 0;
    std::uint64_t samples_seen = 0, dropped_reservations = 0, untracked_transmissions = 0;
    std::uint64_t flow_overflow = 0, node_overflow = 0, delivered = 0, wire_bytes = 0, changes_seen = 0;
    std::uint64_t tcp_payload_bytes = 0, previous_tcp_payload_bytes = 0;
    std::uint64_t ungrouped_flow_payload_bytes = 0, ungrouped_node_payload_bytes = 0;
    Stats previous_stats;
    std::deque<Json> samples, changes;
    std::map<std::uint64_t, LinkRecord> links;
    std::map<std::string, Group> flows, nodes;
    LatencyStatistics latency, window_latency, variation, window_variation;
    explicit Data(const Json& input)
        : config(input), horizon(input.at("until_s")), interval(input.at("interval_s")),
          max_samples(limit(input, "max_samples", 2048, 10000)),
          max_groups(limit(input, "max_groups", 128, 1024)),
          group_retention(limit(input, "group_retention", 128, 1024)),
          max_reservations(limit(input, "max_reservations", 65536, 1000000)),
          latency(limit(input, "latency_retention", 4096, 100000)),
          window_latency(std::min<std::size_t>(limit(input, "latency_retention", 4096, 100000), 256)),
          variation(limit(input, "latency_retention", 4096, 100000)),
          window_variation(std::min<std::size_t>(limit(input, "latency_retention", 4096, 100000), 256)) {}
};
MetricsCollector::MetricsCollector(const Json& config) : data_(std::make_unique<Data>(config)) {}
MetricsCollector::~MetricsCollector() = default;

std::shared_ptr<MetricsCollector> MetricsCollector::attach(SimulationEngine& engine, const Json& config) {
    if (!config.is_object()) throw std::invalid_argument("Analytics config must be an object");
    for (const auto& field : config.items()) {
        if (field.key() != "schema_version" && field.key() != "until_s" && field.key() != "interval_s" &&
            field.key() != "max_samples" && field.key() != "latency_retention" && field.key() != "max_groups" &&
            field.key() != "group_retention" && field.key() != "max_reservations")
            throw std::invalid_argument("Unknown analytics config field: " + field.key());
    }
    if (config.value("schema_version", "") != "1.0") throw std::invalid_argument("Unsupported analytics schema_version");
    for (const char* key : {"until_s", "interval_s"}) {
        if (!config.contains(key) || !config.at(key).is_number() || !std::isfinite(config.at(key).get<double>()) || config.at(key).get<double>() < 0)
            throw std::invalid_argument(std::string(key) + " must be finite and nonnegative");
    }
    const double horizon = config.at("until_s"), interval = config.at("interval_s");
    if (interval <= 0 || !std::isfinite(horizon / interval) || horizon / interval > 10000)
        throw std::invalid_argument("Analytics requires positive interval and at most 10000 intervals");
    auto collector = std::shared_ptr<MetricsCollector>(new MetricsCollector(config));
    auto& d = *collector->data_;
    const auto& links = engine.getTopology().getLinks();
    const auto intervals = static_cast<std::size_t>(std::floor(horizon / interval));
    std::size_t planned = horizon > 0 ? 2 : 1;
    for (std::size_t i = 1; i <= intervals; ++i) if (static_cast<double>(i) * interval < horizon) ++planned;
    const auto retained = std::min(d.max_samples, planned);
    if (!links.empty() && retained > 1000000 / links.size())
        throw std::invalid_argument("Analytics retention exceeds one million link samples");
    for (std::size_t i = 0; i < links.size(); ++i)
        d.links.emplace(links[i]->getId(), LinkRecord{i, links[i]->getMode(), true, {}, Json(), 0, 0, 0, 0, 0});
    engine.setAnalyticsCollector(collector);
    engine.schedule(std::make_unique<SampleEvent>(0, collector));
    for (std::size_t i = 1; i <= intervals; ++i) {
        const double at = static_cast<double>(i) * interval;
        if (at < horizon) engine.schedule(std::make_unique<SampleEvent>(at, collector));
    }
    if (horizon > 0) engine.schedule(std::make_unique<SampleEvent>(horizon, collector));
    return collector;
}
void MetricsCollector::transmission(const Packet& packet, const Link& link, double admitted, double start, double end) {
    auto& d = *data_;
    if (admitted > d.horizon) return;
    const auto found = d.links.find(link.getId());
    if (found == d.links.end()) { ++d.untracked_transmissions; return; }
    auto& record = found->second;
    if (d.reservation_count >= d.max_reservations) {
        record.complete = false; ++d.dropped_reservations; return;
    }
    const int direction = link.getMode() == LinkMode::HALF_DUPLEX ? 2 : packet.current_node == link.getA() ? 0 : 1;
    record.reservations.push_back({start, end, direction});
    ++d.reservation_count;
}
void MetricsCollector::delivery(const Packet& packet, double now) {
    auto& d = *data_;
    if (now > d.horizon) return;
    ++d.delivered;
    d.wire_bytes += static_cast<std::uint64_t>(packet.serializedSize());
    const double latency = now - packet.creation_time;
    const bool data = packet.packet_type == PacketType::DATA;
    if (data) { d.latency.add(latency); d.window_latency.add(latency); }
    const auto update = [&](auto& groups, const std::string& key, std::uint64_t& overflow, bool flow) {
        auto found = groups.find(key);
        if (found == groups.end()) {
            if (groups.size() >= d.max_groups) { ++overflow; return; }
            found = groups.emplace(key, Group(d.group_retention, flow)).first;
        }
        ++found->second.packets;
        found->second.wire_bytes += static_cast<std::uint64_t>(packet.serializedSize());
        if (data) {
            auto& group = found->second;
            group.latency.add(latency);
            if (group.variation) {
                if (group.previous_data_latency) {
                    const double change = std::abs(latency - *group.previous_data_latency);
                    group.variation->add(change);
                    d.variation.add(change); d.window_variation.add(change);
                }
                group.previous_data_latency = latency;
            }
        }
    };
    update(d.flows, flowKey(packet), d.flow_overflow, true);
    update(d.nodes, std::to_string(packet.destination), d.node_overflow, false);
}
void MetricsCollector::tcpPayloadReleased(const Packet& packet, std::uint32_t bytes, double now) {
    auto& d = *data_;
    if (now > d.horizon || bytes == 0) return;
    d.tcp_payload_bytes += bytes;
    const auto update = [bytes](auto& groups, const std::string& key, std::uint64_t& overflow) {
        const auto found = groups.find(key);
        if (found == groups.end()) overflow += bytes;
        else found->second.tcp_payload_bytes += bytes;
    };
    update(d.flows, flowKey(packet), d.ungrouped_flow_payload_bytes);
    update(d.nodes, std::to_string(packet.destination), d.ungrouped_node_payload_bytes);
}
void MetricsCollector::sample(SimulationEngine& engine) {
    auto& d = *data_;
    const double at = engine.now(), duration = at - d.previous_time;
    const auto& stats = engine.getStats();
    const int delivered = stats.packets_delivered - d.previous_stats.packets_delivered;
    const int sent = stats.packets_sent - d.previous_stats.packets_sent;
    const int lost = stats.packets_lost - d.previous_stats.packets_lost;
    if (duration < 0 || delivered < 0 || sent < 0 || lost < 0) throw std::logic_error("Analytics clock/counters moved backwards");
    Json sample{{"at_s", at}, {"from_s", d.previous_time}, {"duration_s", duration},
        {"packets_sent", stats.packets_sent}, {"packets_delivered", stats.packets_delivered}, {"packets_lost", stats.packets_lost},
        {"queue_overflow_drops", stats.queue_overflow_drops}, {"red_early_drops", stats.red_early_drops},
        {"throughput_pps", duration > 0 ? Json(delivered / duration) : Json(nullptr)},
        {"first_hop_admission_pps", duration > 0 ? Json(sent / duration) : Json(nullptr)},
        {"drops_per_s", duration > 0 ? Json(lost / duration) : Json(nullptr)},
        {"tcp_contiguous_payload_bytes", d.tcp_payload_bytes},
        {"tcp_payload_goodput_bps", duration > 0 ? Json(8.0 * (d.tcp_payload_bytes - d.previous_tcp_payload_bytes) / duration) : Json(nullptr)},
        {"data_latency", d.window_latency.toJson()}, {"data_latency_variation", d.window_variation.toJson()}, {"links", Json::array()}};
    std::map<std::uint64_t, std::shared_ptr<Link>> current;
    for (const auto& link : engine.getTopology().getLinks()) current.emplace(link->getId(), link);
    for (auto& [id, record] : d.links) {
        const auto found = current.find(id);
        if (found == current.end()) {
            d.reservation_count -= record.reservations.size();
            record.reservations.clear();
            record.complete = false;
            record.pressure_streak = 0;
            sample["links"].push_back({{"link_index", record.index}, {"present", false},
                {"utilization_defined", false}, {"utilization_complete", false},
                {"utilization", {{"ab", nullptr}, {"ba", nullptr}, {"shared", nullptr}}},
                {"max_queue_fraction", nullptr}});
            continue;
        }
        const auto& link = *found->second;
        std::array<double, 3> busy{};
        for (const auto& range : record.reservations)
            busy[range.direction] += std::max(0.0, std::min(range.end, at) - std::max(range.start, d.previous_time));
        const auto oldSize = record.reservations.size();
        std::erase_if(record.reservations, [at](const auto& range) { return range.end <= at; });
        d.reservation_count -= oldSize - record.reservations.size();
        const bool compatible = link.getMode() == LinkMode::HALF_DUPLEX ? busy[0] == 0 && busy[1] == 0 : busy[2] == 0;
        const bool valid = duration > 0 && record.complete && record.mode == link.getMode() && compatible;
        if (valid) ++record.valid_utilization_windows;
        ++record.queue_samples;
        Json utilization = Json::object();
        double maximum = 0;
        const std::array<const char*, 3> names{"ab", "ba", "shared"};
        for (std::size_t slot = 0; slot < 3; ++slot) {
            const bool enabled = link.getMode() == LinkMode::HALF_DUPLEX ? slot == 2 : slot == 0 || (slot == 1 && link.getMode() == LinkMode::FULL_DUPLEX);
            if (valid && enabled) {
                const double fraction = busy[slot] / duration;
                if (fraction > 1.000000001) throw std::logic_error("Serialized link utilization exceeds capacity");
                utilization[names[slot]] = std::clamp(fraction, 0.0, 1.0);
                maximum = std::max(maximum, std::clamp(fraction, 0.0, 1.0));
            } else utilization[names[slot]] = nullptr;
        }
        const auto ab = link.estimatedQueueSize(at, link.getA(), link.getB());
        const auto ba = link.getMode() == LinkMode::FULL_DUPLEX ? link.estimatedQueueSize(at, link.getB(), link.getA()) : 0;
        const double queue = static_cast<double>(std::max(ab, ba)) / static_cast<double>(link.getQueueCapacity());
        if (valid) record.valid_duration += duration;
        const bool pressure = valid && maximum >= 0.9 && queue >= 0.8;
        if (pressure) {
            ++record.pressure_windows; record.high_pressure_duration += duration;
            if (record.pressure_streak == 0) record.pressure_start = d.previous_time;
            ++record.pressure_streak;
            if (record.pressure_streak > record.longest_pressure_streak) {
                record.longest_pressure_streak = record.pressure_streak;
                record.longest_pressure_from = record.pressure_start; record.longest_pressure_to = at;
            }
        } else record.pressure_streak = 0;
        if (maximum > record.peak_utilization) { record.peak_utilization = maximum; record.util_from = d.previous_time; record.util_to = at; }
        if (queue > record.peak_queue) { record.peak_queue = queue; record.queue_at = at; }
        Json state{{"up", link.isUp()}, {"bandwidth_mbps", link.getBandwidthMbps()}, {"delay_ms", link.getDelayMs()},
            {"loss_probability", link.getLossProb()}, {"queue_capacity_per_slot", link.getQueueCapacity()},
            {"queue_policy", link.getQueueDiscipline() == Link::QueueDiscipline::RED ? "red" : "drop_tail"},
            {"mode", link.getMode() == LinkMode::FULL_DUPLEX ? "full_duplex" : link.getMode() == LinkMode::HALF_DUPLEX ? "half_duplex" : "simplex"}};
        if (!record.previous.is_null() && record.previous != state) {
            if (d.changes.size() == d.max_samples) d.changes.pop_front();
            d.changes.push_back({{"observed_at_s", at}, {"link_index", record.index}, {"before", record.previous}, {"after", state}});
            ++d.changes_seen;
        }
        record.previous = state; record.mode = link.getMode();
        sample["links"].push_back({{"link_index", record.index}, {"present", true}, {"state", state}, {"utilization", utilization},
            {"utilization_complete", record.complete}, {"utilization_defined", valid}, {"queue_ab_or_shared", ab}, {"queue_ba", ba},
            {"queue_capacity_per_slot", link.getQueueCapacity()}, {"max_queue_fraction", queue}});
    }
    if (d.samples.size() == d.max_samples) d.samples.pop_front();
    d.samples.push_back(std::move(sample)); ++d.samples_seen;
    d.previous_time = at; d.previous_stats = stats;
    d.previous_tcp_payload_bytes = d.tcp_payload_bytes;
    d.window_latency = LatencyStatistics(std::min<std::size_t>(d.config.value("latency_retention", 4096), 256));
    d.window_variation = LatencyStatistics(std::min<std::size_t>(d.config.value("latency_retention", 4096), 256));
}
Json MetricsCollector::toJson() const {
    const auto& d = *data_;
    const auto groups = [](const auto& input) {
        Json output = Json::array();
        for (const auto& [key, group] : input) {
            Json item{{"key", key}, {"packets_delivered", group.packets}, {"wire_bytes_delivered", group.wire_bytes},
                      {"data_latency", group.latency.toJson()}, {"tcp_contiguous_payload_bytes", group.tcp_payload_bytes}};
            if (group.variation) item["data_latency_variation"] = group.variation->toJson();
            output.push_back(std::move(item));
        }
        return output;
    };
    Json recommendations = Json::array();
    Json linkSummaries = Json::array();
    for (const auto& [id, record] : d.links) {
        (void)id;
        linkSummaries.push_back({{"link_index", record.index}, {"reservation_coverage_complete", record.complete},
            {"valid_utilization_windows", record.valid_utilization_windows},
            {"valid_utilization_duration_s", record.valid_duration}, {"pressure_windows", record.pressure_windows},
            {"pressure_duration_s", record.high_pressure_duration}, {"longest_pressure_streak_windows", record.longest_pressure_streak},
            {"max_sampled_directional_utilization", record.valid_utilization_windows ? Json(record.peak_utilization) : Json(nullptr)},
            {"max_sampled_directional_queue_fraction", record.queue_samples ? Json(record.peak_queue) : Json(nullptr)}});
        if (record.peak_utilization >= 0.9) recommendations.push_back({{"rule", "high_serialization_utilization"},
            {"classification", "heuristic"}, {"link_index", record.index},
            {"evidence", {{"max_directional_fraction", record.peak_utilization}, {"threshold", 0.9}, {"from_s", record.util_from}, {"to_s", record.util_to}}},
            {"recommendation", "Compare traffic demand and bandwidth alternatives under the same workload."},
            {"limitations", "A sampled high-utilization interval alone does not establish a sustained bottleneck."}});
        if (record.peak_queue >= 0.8) recommendations.push_back({{"rule", "high_queue_occupancy"},
            {"classification", "heuristic"}, {"link_index", record.index},
            {"evidence", {{"max_directional_queue_fraction", record.peak_queue}, {"threshold", 0.8}, {"observed_at_s", record.queue_at}}},
            {"recommendation", "Compare queue policy/capacity and propagation delay before changing buffer sizes."},
            {"limitations", "Current queue entries include propagation; this is not a physical waiting-buffer measurement."}});
        if (record.longest_pressure_streak >= 3) recommendations.push_back({{"rule", "consecutive_serialization_queue_pressure"},
            {"classification", "heuristic"}, {"link_index", record.index},
            {"evidence", {{"consecutive_windows", record.longest_pressure_streak}, {"minimum_windows", 3},
                {"utilization_threshold", 0.9}, {"queue_threshold", 0.8},
                {"from_s", record.longest_pressure_from}, {"to_s", record.longest_pressure_to},
                {"total_observed_pressure_s", record.high_pressure_duration}}},
            {"recommendation", "Investigate repeated serialization/queue pressure with demand, bandwidth and sampling-interval sweeps."},
            {"limitations", "Three sampled windows are interval-dependent evidence, not proof of sustained overload or its cause; queue occupancy includes propagation."}});
    }
    if (d.previous_stats.packets_lost > 0) recommendations.push_back({{"rule", "observed_path_drops"}, {"classification", "heuristic"},
        {"evidence", {{"packets_lost", d.previous_stats.packets_lost}, {"queue_overflow_drops", d.previous_stats.queue_overflow_drops}, {"red_early_drops", d.previous_stats.red_early_drops}}},
        {"recommendation", "Inspect availability and loss configuration; run controlled loss and queue experiments."},
        {"limitations", "Aggregate path drops do not identify a flow or establish an abnormal loss probability."}});
    return {{"schema_version", "1.0"}, {"config", d.config}, {"collection_until_s", d.horizon},
        {"definitions", {{"time_unit", "virtual_seconds"}, {"packet_rate_unit", "packets_per_virtual_second"},
            {"latency_scope", "delivered packets classified DATA, including retransmissions, at time <= until_s"},
            {"latency_variation", "absolute difference of successive DATA delivery latencies within each tracked flow/direction; not RFC jitter or application goodput"},
            {"tcp_payload_goodput", "unique contiguous TCP payload bytes released by modeled receiver, excluding headers/control/duplicates; bits per virtual second; buffered bytes counted when gap closes, not application reads"},
            {"sample_boundaries", "event-ID order; horizon-tied deliveries may update aggregates after the final sample"},
            {"utilization", "clipped serialization busy seconds per directional/shared slot divided by sample duration; propagation excluded"},
            {"queue_scope", "accepted transmissions until arrival, including propagation"},
            {"flow_key_fields", {"session_id", "source", "destination", "source_port", "destination_port"}},
            {"node_group_scope", "destination_deliveries"}, {"wire_bytes", "serialized delivered bytes, not unique application goodput"}}},
        {"samples_seen", d.samples_seen}, {"samples_discarded", d.samples_seen - d.samples.size()}, {"samples", d.samples},
        {"link_changes_observed", d.changes_seen}, {"link_changes_discarded", d.changes_seen - d.changes.size()}, {"link_changes", d.changes},
        {"packets_delivered_in_scope", d.delivered}, {"wire_bytes_delivered_in_scope", d.wire_bytes},
        {"tcp_contiguous_payload_bytes", d.tcp_payload_bytes},
        {"tcp_payload_goodput_bps", d.horizon > 0 ? Json(8.0 * d.tcp_payload_bytes / d.horizon) : Json(nullptr)},
        {"ungrouped_flow_tcp_payload_bytes", d.ungrouped_flow_payload_bytes},
        {"ungrouped_node_tcp_payload_bytes", d.ungrouped_node_payload_bytes},
        {"data_latency", d.latency.toJson()}, {"flow_groups", groups(d.flows)}, {"destination_node_groups", groups(d.nodes)},
        {"data_latency_variation", d.variation.toJson()}, {"latency_variation_coverage", "tracked_flow_groups_only"},
        {"ungrouped_flow_deliveries", d.flow_overflow}, {"ungrouped_node_deliveries", d.node_overflow},
        {"reservation_records_discarded", d.dropped_reservations}, {"untracked_link_transmissions", d.untracked_transmissions},
        {"link_summaries", linkSummaries},
        {"recommendations", recommendations}};
}
}
