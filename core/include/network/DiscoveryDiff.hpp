#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace kns {

struct DiscoveryDiff {
    bool baseline_available = false;
    std::vector<std::string> added_nodes;
    std::vector<std::string> removed_nodes;
    std::vector<std::string> changed_nodes;
    std::vector<std::string> added_links;
    std::vector<std::string> removed_links;
    std::vector<std::string> changed_links;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::string summary() const;
};

[[nodiscard]] DiscoveryDiff parseDiscoveryDiff(const nlohmann::json& document);

} // namespace kns
