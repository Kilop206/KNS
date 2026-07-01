#include <algorithm>
#include <iostream>

#include "../include/VisualPacketManager.hpp"
#include "../../../core/include/network/Packet.hpp"

namespace interface {
    void VisualPacketManager::observePacket(
        const kns::Packet& packet,
        std::uint64_t session_id,
        int from,
        int to,
        double departure_time,
        double arrival_time
    ) {
        double visual_duration = std::max(0.03, (arrival_time - departure_time) * 2.0);
        VisualPacket p{
            from,
            to,
            packet.packet_type,
            session_id,
            departure_time,
            arrival_time,
            departure_time,
            std::max(0.001, arrival_time - departure_time)
        };

        pending_.push_back(p);
    }

    void VisualPacketManager::update(double visual_time) {
        for (auto& p : pending_) {
            p.visual_start_time = std::max(visual_time, next_spawn_time_);
            next_spawn_time_ = p.visual_start_time + spawn_gap_;
            active_.push_back(p);
        }
        pending_.clear();

        active_.erase(
            std::remove_if(
                active_.begin(),
                active_.end(),
                [visual_time](const VisualPacket& p) {
                    return visual_time >= (p.visual_start_time + p.visual_duration);
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