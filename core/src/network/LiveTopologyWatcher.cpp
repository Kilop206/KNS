#include "network/LiveTopologyWatcher.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

#include "network/TopologyLoader.hpp"

namespace kns {

void LiveTopologyWatcher::setSource(std::string path, bool enabled)
{
    if (path_ == path && enabled_ == enabled) return;
    path_ = std::move(path);
    enabled_ = enabled;
    ++generation_;
    contents_.clear();
    error_.clear();
    next_check_ = {};
}

std::optional<Topology> LiveTopologyWatcher::poll(bool force)
{
    if (pending_.valid()) {
        if (pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return std::nullopt;
        auto result = pending_.get();
        ++completed_checks_;
        if (pending_generation_ == generation_) {
            error_ = std::move(result.error);
            if (error_.empty()) contents_ = std::move(result.contents);
            return std::move(result.topology);
        }
    }
    const auto now = std::chrono::steady_clock::now();
    if (!enabled_ || path_.empty() || (!force && now < next_check_)) return std::nullopt;
    next_check_ = now + std::chrono::seconds(1);
    pending_generation_ = generation_;
    pending_ = std::async(std::launch::async, [path = path_, previous = contents_] {
        Result result;
        try {
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Cannot open live topology file");
            constexpr std::size_t maximum = 4 * 1024 * 1024;
            std::string contents(maximum + 1, '\0');
            file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
            contents.resize(static_cast<std::size_t>(file.gcount()));
            if (file.bad()) throw std::runtime_error("Cannot read live topology file");
            if (contents.size() > maximum) throw std::runtime_error("Live topology exceeds 4 MiB");
            if (contents.empty()) throw std::runtime_error("Live topology file is empty");
            if (contents != previous) result.topology = TopologyLoader::fromJson(nlohmann::json::parse(contents));
            result.contents = std::move(contents);
        } catch (const std::exception& exception) {
            result.error = exception.what();
        }
        return result;
    });
    return std::nullopt;
}

} // namespace kns
