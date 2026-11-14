#pragma once
#include <memory>
#include <nlohmann/json.hpp>

namespace kns { class SimulationEngine; struct Packet; }
namespace kns::traffic {
// Bounded, one-event-per-active-flow generators using the existing network model.
class TrafficRun : public std::enable_shared_from_this<TrafficRun> {
public:
    static std::shared_ptr<TrafficRun> schedule(SimulationEngine&, const nlohmann::json&);
    ~TrafficRun();
    nlohmann::json toJson(double simulation_time_s) const;
    void generate(SimulationEngine&, std::size_t flow_index);
    void replay(SimulationEngine&, std::size_t event_index);
    void respond(SimulationEngine&, std::size_t flow_index, double operation_started_s);
    void delivered(SimulationEngine&, std::size_t flow_index, const Packet&, bool response, double operation_started_s);
    void rejected(std::size_t flow_index, bool response) noexcept;
private:
    struct Data;
    explicit TrafficRun(const nlohmann::json&);
    std::unique_ptr<Data> data_;
};
}
