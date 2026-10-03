#include "network/TopologyLoader.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace kns {
namespace {
namespace fs = std::filesystem;

// Reserving a directory avoids sharing a temporary file with another writer.
// Keep it beside the destination so the final rename stays on one filesystem.
class StagedTopology {
public:
    explicit StagedTopology(const fs::path& parent)
    {
        static std::atomic<unsigned long long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 64; ++attempt) {
            directory_ = parent / (".kns-save-" + std::to_string(timestamp) + "-" +
                                   std::to_string(sequence.fetch_add(1)));
            if (fs::create_directory(directory_)) return;
        }
        throw std::runtime_error("Cannot reserve a temporary topology file");
    }

    StagedTopology(const StagedTopology&) = delete;
    StagedTopology& operator=(const StagedTopology&) = delete;

    ~StagedTopology()
    {
        std::error_code error;
        fs::remove(file(), error);
        fs::remove(directory_, error);
    }

    fs::path file() const { return directory_ / "topology.json"; }

private:
    fs::path directory_;
};
} // namespace

void TopologyLoader::save_topology(const Topology& topology, const std::string& filename)
{
    if (filename.empty()) throw std::invalid_argument("Choose a topology file to save");
    const auto document = toJson(topology);
    // Refuse to overwrite a good file with a snapshot the loader cannot reopen.
    (void)fromJson(document);
    const auto contents = document.dump(2) + "\n";
    const auto target = fs::absolute(fs::path(std::u8string(filename.begin(), filename.end())));
    StagedTopology staged(target.parent_path());
    std::ofstream output;
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output.open(staged.file(), std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();

#ifdef _WIN32
    if (!MoveFileExW(staged.file().c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error = GetLastError();
        throw fs::filesystem_error("Cannot replace topology file", staged.file(), target,
            std::error_code(static_cast<int>(error), std::system_category()));
    }
#else
    fs::rename(staged.file(), target);
#endif
}
} // namespace kns
