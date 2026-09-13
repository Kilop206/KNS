#pragma once

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
    };

} // namespace gui
