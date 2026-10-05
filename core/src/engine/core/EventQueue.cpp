#include "engine/core/EventQueue.hpp"
#include "engine/core/Event.hpp"

#include <stdexcept>
#include <limits>
#include <iostream>
#include <algorithm>

namespace kns {
    // Comparator for the priority queue to order events by timestamp and ID
    bool EventQueue::EventComparator::operator()(const std::unique_ptr<Event>& a,
        const std::unique_ptr<Event>& b) const {
        // Order by timestamp first (earlier events have higher priority)
        if (a->getTimestamp() != b->getTimestamp()) {
            return a->getTimestamp() > b->getTimestamp();
        }

        // If timestamps are equal, order by ID (lower ID has higher priority)
        return a->getId() > b->getId();
    }

    // Schedule a new event in the queue
    void EventQueue::schedule(std::unique_ptr<Event> event) {
        // Ensure the event is not null before scheduling
        if (!event) {
            throw std::invalid_argument("Cannot schedule null event");
        }

        // Add the event to the priority queue
        event_list_.push_back(std::move(event));
        std::push_heap(event_list_.begin(), event_list_.end(), EventComparator{});
    }

    // Get the next event from the queue, or return nullptr if the queue is empty
    std::unique_ptr<Event> EventQueue::next() {
        if (event_list_.empty()) return nullptr;

        // Get the event with the earliest timestamp (and lowest ID if timestamps are equal)
        std::pop_heap(event_list_.begin(), event_list_.end(), EventComparator{});
        auto ptr = std::move(event_list_.back());
        event_list_.pop_back();
        return ptr;
    }

    // Check if there are any events in the queue
    bool EventQueue::hasEvents() const noexcept
    {
        return !event_list_.empty();
    }

    // Get the number of events currently in the queue
    std::size_t EventQueue::size() const noexcept
    {
        return event_list_.size();
    }


    double EventQueue::peekTimestamp() const noexcept
    {
        if (event_list_.empty()) {
            return std::numeric_limits<double>::infinity();
        }

        return event_list_.front()->getTimestamp();
    }


    // Clear all events from the queue
    void EventQueue::clear()
    {
        event_list_.clear();
    }

    bool EventQueue::cancel(std::uint64_t id) {
        const auto found = std::find_if(event_list_.begin(), event_list_.end(),
            [id](const auto& event) { return event->getId() == id; });
        if (found == event_list_.end()) return false;
        event_list_.erase(found);
        std::make_heap(event_list_.begin(), event_list_.end(), EventComparator{});
        return true;
    }
}
