#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace kns {
    class Topology;
}

namespace gui {
    class TranslationService;

    struct DeviceSearchResults {
        std::vector<int> nodes;
        std::size_t total = 0;
    };

    /// ASCII case-insensitive tokens match name, type or identity/address fields.
    /// #ID and type:name are exact filters. All tokens must match; only active nodes qualify.
    DeviceSearchResults findDevices(const kns::Topology& topology, std::string_view query,
                                    std::size_t limit = 100);

    class DeviceFinder {
    public:
        /// Render the "device-finder" popup in the caller's current window ID scope.
        std::optional<int> render(const kns::Topology& topology, TranslationService& translations);

    private:
        std::array<char, 256> query_{};
        int highlighted_ = -1;
    };
}
