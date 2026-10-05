#include "network/DiscoveryDiff.hpp"

#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace kns {
namespace {

std::vector<std::string> readIdentityList(
    const nlohmann::json& document,
    const char* key
)
{
    const auto iterator = document.find(key);
    if (iterator == document.end()) return {};
    if (!iterator->is_array()) {
        throw std::invalid_argument(std::string(key) + " must be an array");
    }

    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    result.reserve(iterator->size());
    for (const auto& value : *iterator) {
        if (!value.is_string() || value.get_ref<const std::string&>().empty()) {
            throw std::invalid_argument(std::string(key) + " must contain non-empty strings");
        }
        auto identity = value.get<std::string>();
        if (!seen.insert(identity).second) {
            throw std::invalid_argument(std::string(key) + " must not contain duplicates");
        }
        result.push_back(std::move(identity));
    }
    return result;
}

} // namespace

DiscoveryDiff parseDiscoveryDiff(const nlohmann::json& document)
{
    if (!document.is_object()) {
        throw std::invalid_argument("Discovery diff must be an object");
    }
    if (document.value("schema_version", "") != "1.0") {
        throw std::invalid_argument("Unsupported discovery diff schema_version");
    }
    const auto baseline = document.find("baseline_available");
    if (baseline == document.end() || !baseline->is_boolean()) {
        throw std::invalid_argument("Discovery diff requires boolean baseline_available");
    }

    static const std::unordered_set<std::string> allowed{
        "schema_version",
        "baseline_available",
        "added_nodes",
        "removed_nodes",
        "changed_nodes",
        "added_links",
        "removed_links",
        "changed_links",
    };
    for (const auto& [key, value] : document.items()) {
        (void)value;
        if (!allowed.contains(key)) {
            throw std::invalid_argument("Unknown discovery diff field: " + key);
        }
    }

    DiscoveryDiff result;
    result.baseline_available = baseline->get<bool>();
    result.added_nodes = readIdentityList(document, "added_nodes");
    result.removed_nodes = readIdentityList(document, "removed_nodes");
    result.changed_nodes = readIdentityList(document, "changed_nodes");
    result.added_links = readIdentityList(document, "added_links");
    result.removed_links = readIdentityList(document, "removed_links");
    result.changed_links = readIdentityList(document, "changed_links");
    return result;
}

bool DiscoveryDiff::empty() const noexcept
{
    return added_nodes.empty() &&
        removed_nodes.empty() &&
        changed_nodes.empty() &&
        added_links.empty() &&
        removed_links.empty() &&
        changed_links.empty();
}

std::string DiscoveryDiff::summary() const
{
    std::ostringstream output;
    if (!baseline_available) {
        output << "Initial discovery snapshot";
    } else if (empty()) {
        output << "No discovery changes";
    } else {
        output << "Discovery changes";
    }
    output
        << ": nodes +" << added_nodes.size()
        << " / -" << removed_nodes.size()
        << " / ~" << changed_nodes.size()
        << ", links +" << added_links.size()
        << " / -" << removed_links.size()
        << " / ~" << changed_links.size();
    return output.str();
}

} // namespace kns
