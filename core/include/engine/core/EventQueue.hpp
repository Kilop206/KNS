#pragma once

#include <memory>
#include <queue>
#include <vector>

#include "engine/events/Event.hpp"

namespace kns {

    class EventQueue {
    public:
		// Adds an event to the queue. The event will be processed in the order it was scheduled.
        void schedule(std::unique_ptr<Event> event);

		// Retrieves and removes the next event from the queue. The event with the earliest timestamp will be returned first.
        std::unique_ptr<kns::Event> next();

		// Checks if the event queue is empty. Returns true if there are no events in the queue, false otherwise.
        bool hasEvents() const noexcept;

		// Returns the number of events currently in the queue.
        std::size_t size() const noexcept;

		// Returns the timestamp of the next event without removing it.
        double peekTimestamp() const noexcept;

		// Clears all events from the queue, resetting it to an empty state.
        void clear();

    private:

		// Comparator for the priority queue to order events by their timestamp. Events with earlier timestamps will be processed first.
        struct EventComparator {

			// Compares two events based on their timestamps. Returns true if the timestamp of event a is greater than that of event b, indicating that event a should be processed after event b.
            bool operator()(const std::unique_ptr<Event>& a,
                const std::unique_ptr<Event>& b) const;
        };

		// The priority queue that holds the events, ordered by their timestamps. The event with the earliest timestamp will be at the top of the queue.
        std::priority_queue<
            std::unique_ptr<Event>,
            std::vector<std::unique_ptr<kns::Event>>,
            EventComparator
        > event_list_;
    };

}