#include "network/DiscoveryDiffWatcher.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace kns {

void DiscoveryDiffWatcher::setSource(std::string path, bool enabled)
{
    if (path_ == path && enabled_ == enabled) return;
    path_ = std::move(path);
    enabled_ = enabled;
    ++generation_;
    contents_.clear();
    error_.clear();
    next_check_ = {};
}

std::optional<DiscoveryDiff> DiscoveryDiffWatcher::poll(bool force)
{
    if (pending_.valid()) {
        if (pending_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return std::nullopt;
        }
        auto result = pending_.get();
        ++completed_checks_;
        if (pending_generation_ == generation_) {
            error_ = std::move(result.error);
            if (error_.empty()) contents_ = std::move(result.contents);
            return std::move(result.diff);
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (!enabled_ || path_.empty() || (!force && now < next_check_)) {
        return std::nullopt;
    }

    next_check_ = now + std::chrono::seconds(1);
    pending_generation_ = generation_;
    pending_ = std::async(std::launch::async, [path = path_, previous = contents_] {
        Result result;
        try {
            std::ifstream file(
                std::filesystem::path(std::u8string(path.begin(), path.end())),
                std::ios::binary
            );
            if (!file) throw std::runtime_error("Cannot open discovery diff file");

            constexpr std::size_t maximum = 256 * 1024;
            std::string contents(maximum + 1, '\0');
            file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
            contents.resize(static_cast<std::size_t>(file.gcount()));
            if (file.bad()) throw std::runtime_error("Cannot read discovery diff file");
            if (contents.size() > maximum) throw std::runtime_error("Discovery diff exceeds 256 KiB");
            if (contents.empty()) throw std::runtime_error("Discovery diff file is empty");

            if (contents != previous) {
                result.diff = parseDiscoveryDiff(nlohmann::json::parse(contents));
            }
            result.contents = std::move(contents);
        } catch (const std::exception& exception) {
            result.error = exception.what();
        }
        return result;
    });
    return std::nullopt;
}

} // namespace kns
