#include "engine/events/LinkFailureEvent.hpp"

#include "engine/core/SimulationEngine.hpp"
#include "engine/core/Log.hpp"

namespace kns {

    LinkFailureEvent::LinkFailureEvent(double timestamp, int node_a, int node_b, bool up)
        : Event(timestamp),
          node_a_(node_a),
          node_b_(node_b),
          up_(up)
    {
    }

    void LinkFailureEvent::execute(SimulationEngine& engine)
    {
        const bool changed = engine.toggleLinkUp(node_a_, node_b_, up_);

        if (!changed) {
            KNS_DEBUG_LOG(
                "[LinkFailure] No link found between nodes "
                << node_a_ << " and " << node_b_
                << " — event ignored\n");
            return;
        }

        KNS_DEBUG_LOG(
            "[LinkFailure] Link "
            << node_a_ << "<->" << node_b_
            << (up_ ? " restored" : " failed")
            << " at t=" << getTimestamp()
            << '\n');
    }

}
