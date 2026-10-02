#pragma once

#include <array>
#include <string>

namespace kns {
    class SimulationEngine;
}

namespace gui {

    class TranslationService;

    class TopologyPanel {
    public:
        void render(
            kns::SimulationEngine& engine,
            TranslationService& translations
        );

    private:
        std::array<char, 256> new_label_{};
        int new_type_ = 1;
        int link_from_ = 0;
        int link_to_ = 1;
        std::string error_;
    };

} // namespace gui
