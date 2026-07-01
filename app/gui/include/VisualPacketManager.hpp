#include <vector>

#include "../../../core/include/network/Packet.hpp"
#include "VisualPacket.hpp"

namespace interface {
    class VisualPacketManager {
        private:
            std::vector<VisualPacket> pending_;
            std::vector<VisualPacket> active_;

            double next_spawn_time_ = 0.0;
            double spawn_gap_ = 0.005;
            double minimum_duration_ = 0.35;

        public:
            void observePacket(
                const kns::Packet& packet,
                std::uint64_t session_id,
                int from,
                int to,
                double departure_time,
                double arrival_time
            );

            void update(double visual_time);

            const std::vector<VisualPacket>& getActivePackets() const;
            void clear();
    };
}