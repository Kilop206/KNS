#include <algorithm>
#include <iostream>

#include "../include/VisualPacketManager.hpp"
#include "../../../core/include/network/Packet.hpp"

namespace gui {

    void VisualPacketManager::observePacket(
        const kns::Packet& packet,
        std::uint64_t session_id,
        int from,
        int to,
        double departure_time,
        double arrival_time
    ) {
        pending_.push_back(VisualPacket{
            from,
            to,
            kns::inferPacketType(packet.tcp),
            session_id,
            departure_time,
            arrival_time,
            departure_time,
            std::max(0.001, arrival_time - departure_time)
        });
    }

    void VisualPacketManager::update(double visual_time) {
        for (auto& p : pending_) {
            active_.push_back(p);
        }
        pending_.clear();

        active_.erase(
            std::remove_if(
                active_.begin(),
                active_.end(),
                [visual_time](const VisualPacket& p) {
                    return visual_time >= p.sim_arrival_time;
                }
            ),
            active_.end()
        );
    }

    const std::vector<VisualPacket>& VisualPacketManager::getActivePackets() const {
        return active_;
    }

    void VisualPacketManager::clear() {
        pending_.clear();
        active_.clear();
        next_spawn_time_ = 0.0;
    }

}