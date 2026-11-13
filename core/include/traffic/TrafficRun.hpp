#pragma once
#include <memory>
#include <nlohmann/json.hpp>

namespace kns { class SimulationEngine; }
namespace kns::traffic {
// Bounded, one-event-per-active-flow generators using the existing network model.
class TrafficRun : public std::enable_shared_from_this<TrafficRun> {
public:
    static std::shared_ptr<TrafficRun> schedule(SimulationEngine&, const nlohmann::json&);
    ~TrafficRun();
    nlohmann::json toJson(double simulation_time_s) const;
    void generate(SimulationEngine&, std::size_t flow_index);
private:
    struct Data;
    explicit TrafficRun(const nlohmann::json&);
    std::unique_ptr<Data> data_;
};
}
