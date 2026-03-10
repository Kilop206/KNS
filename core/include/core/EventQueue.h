#pragma once

#include <memory>
#include <queue>
#include <vector>

#include "core/Event.h"

namespace core {

    class EventQueue {
    public:
        void schedule(std::unique_ptr<Event> event);
        std::unique_ptr<Event> next();

        bool hasEvents() const noexcept;
        std::size_t size() const noexcept;
        void clear();

    private:
        struct EventComparator {
            bool operator()(const std::unique_ptr<Event>& a,
                const std::unique_ptr<Event>& b) const;
        };

        std::priority_queue<
            std::unique_ptr<Event>,
            std::vector<std::unique_ptr<Event>>,
            EventComparator
        > event_list_;
    };

}