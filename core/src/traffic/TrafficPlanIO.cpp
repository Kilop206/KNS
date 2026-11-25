#include "traffic/TrafficPlanIO.hpp"
#include "traffic/TrafficRun.hpp"
#include <algorithm>
#include <array>
#include <istream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace kns::traffic {
    nlohmann::json parseTrafficPlan(std::string_view text, const SimulationEngine& engine) {
        if (text.size() > MAX_PLAN_BYTES)
            throw std::invalid_argument("Traffic plan exceeds 4 MiB");
        using Json = nlohmann::json;
        std::vector<std::set<std::string>> objects;
        auto callback = [&](int depth, Json::parse_event_t event, Json& value) {
            if (depth > MAX_PLAN_DEPTH)
                throw std::invalid_argument("Traffic plan exceeds maximum JSON nesting depth");
            if (event == Json::parse_event_t::object_start)
                objects.emplace_back();
            else if (event == Json::parse_event_t::object_end)
                objects.pop_back();
            else if (event == Json::parse_event_t::key &&
                     !objects.back().insert(value.get<std::string>()).second)
                throw std::invalid_argument("Duplicate traffic plan JSON key: " + value.get<std::string>());
            return true;
        };
        auto plan = Json::parse(text.begin(), text.end(), callback);
        TrafficRun::validatePlan(engine, plan);
        return plan;
    }

    nlohmann::json readTrafficPlan(std::istream& input, const SimulationEngine& engine) {
        std::string text;
        std::array<char, 8192> buffer;
        for (;;) {
            // Read at most limit + 1 bytes, even if the file changes after opening.
            const auto count = std::min(buffer.size(), MAX_PLAN_BYTES + 1 - text.size());
            input.read(buffer.data(), static_cast<std::streamsize>(count));
            text.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
            if (text.size() > MAX_PLAN_BYTES)
                throw std::invalid_argument("Traffic plan exceeds 4 MiB");
            if (input.bad() || (input.fail() && !input.eof()))
                throw std::runtime_error("Cannot read traffic plan");
            if (input.eof())
                break;
        }
        return parseTrafficPlan(text, engine);
    }
}
