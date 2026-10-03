#pragma once

#include <utility>
#include <vector>
<<<<<<< HEAD

#include "imgui.h"

=======
#include "imgui.h"

#include "../include/VisualPacket.hpp"
#include "enums/PacketType.hpp"

>>>>>>> 879e9a30eb706359e007b3218a4c881c257cd5bc
namespace kns {
    class SimulationEngine;
    class Topology;
}

<<<<<<< HEAD
namespace interface {

class PacketRenderer {
public:
    void render(
        ImDrawList* draw_list,
        const kns::Topology& topo,
        const std::vector<std::pair<float, float>>& positions,
        kns::SimulationEngine& engine,
        double minimum_visible_duration_seconds = 0.35
    ) const;
};

} // namespace interface
=======
namespace gui {

    class TranslationService;

    class PacketRenderer {
        public:
            void render(
                ImDrawList* draw_list,
                const std::vector<std::pair<float, float>>& positions,
                const std::vector<VisualPacket>& packets,
                double visual_time,
                TranslationService& translations
            ) const;

            static const char* packetTypeToString(kns::PacketType type);
            
            static ImU32 packetColorByType(kns::PacketType type);

        private:
            static ImU32 packetBorderColor(kns::PacketType type);
        };
}
>>>>>>> 879e9a30eb706359e007b3218a4c881c257cd5bc
