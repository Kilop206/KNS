#include "engine/events/LinkFailureEvent.hpp"

#include "engine/core/SimulationEngine.hpp"
#include "engine/core/Log.hpp"

namespace kns {

    LinkFailureEvent::LinkFailureEvent(double timestamp, int node_a, int node_b, bool up)
        : Event(timestamp),
          node_a_(node_a),
          node_b_(node_b),
          link_id_(0),
          target_by_id_(false),
          up_(up)
    {
    }

    LinkFailureEvent::LinkFailureEvent(
        double timestamp,
        std::uint64_t link_id,
        bool up
    )
        : Event(timestamp),
          node_a_(-1),
          node_b_(-1),
          link_id_(link_id),
          target_by_id_(true),
          up_(up)
    {
    }

    void LinkFailureEvent::execute(SimulationEngine& engine)
    {
        const bool changed = target_by_id_
            ? engine.toggleLinkUpById(link_id_, up_)
            : engine.toggleLinkUp(node_a_, node_b_, up_);

        if (!changed) {
            if (target_by_id_) {
                KNS_DEBUG_LOG(
                    "[LinkFailure] No link found with id "
                    << link_id_ << " — event ignored\n");
            } else {
                KNS_DEBUG_LOG(
                    "[LinkFailure] No unique link found between nodes "
                    << node_a_ << " and " << node_b_
                    << " — event ignored\n");
            }
            return;
        }

        if (target_by_id_) {
            KNS_DEBUG_LOG(
                "[LinkFailure] Link id=" << link_id_
                << (up_ ? " restored" : " failed")
                << " at t=" << getTimestamp()
                << '\n');
        } else {
            KNS_DEBUG_LOG(
                "[LinkFailure] Link "
                << node_a_ << "<->" << node_b_
                << (up_ ? " restored" : " failed")
                << " at t=" << getTimestamp()
                << '\n');
        }
    }

}
