#pragma once

namespace kns {
    class SimulationEngine;
}

namespace gui {

    class TopologyPanel {
    public:
        void render(kns::SimulationEngine& engine);
    };

} // namespace gui
