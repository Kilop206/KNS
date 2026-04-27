#include <cstdlib>
#include <stdexcept>

#include "network/Link.hpp"

namespace kns {
    int Link::getOtherNode(int node) const {
        if (node == to) {
            return from;
        } else if (node == from) {
            return to;
        } else {
            throw std::runtime_error("Invalid node");
        }
    }

    bool Link::should_drop() const {
        return ((double) rand() / RAND_MAX) <= loss_prob;
    }
}