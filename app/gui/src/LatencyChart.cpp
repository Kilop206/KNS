#include "../include/LatencyChart.hpp"

#include <algorithm>
#include <numeric>

namespace interface {
    CircularBuffer::CircularBuffer(std::size_t capacity)
        : capacity_(capacity == 0 ? 1 : capacity),
          buffer_(capacity_ > 0 ? capacity_ : 1, 0.0f) {}

    void CircularBuffer::recomputeStatistics() {
        if (count_ == 0) {
            minimum_latency_ = 0.0f;
            maximum_latency_ = 0.0f;
            avg_latency_ = 0.0f;
            return;
        }

        const std::vector<float> snapshot = values();

        minimum_latency_ = *std::min_element(snapshot.begin(), snapshot.end());
        maximum_latency_ = *std::max_element(snapshot.begin(), snapshot.end());

        const float sum = std::accumulate(snapshot.begin(), snapshot.end(), 0.0f);
        avg_latency_ = sum / static_cast<float>(snapshot.size());
    }

    void CircularBuffer::addLatencyToBuffer(float latency) {
        if (count_ < capacity_) {
            buffer_[(start_ + count_) % capacity_] = latency;
            ++count_;
        } else {
            buffer_[start_] = latency;
            start_ = (start_ + 1) % capacity_;
        }

        recomputeStatistics();
    }

    std::vector<float> CircularBuffer::values() const {
        std::vector<float> ordered;
        ordered.reserve(count_);

        for (std::size_t i = 0; i < count_; ++i) {
            ordered.push_back(buffer_[(start_ + i) % capacity_]);
        }

        return ordered;
    }

    float CircularBuffer::getAverageLatency() const {
        return avg_latency_;
    }

    float CircularBuffer::getMinimumLatency() const {
        return minimum_latency_;
    }

    float CircularBuffer::getMaximumLatency() const {
        return maximum_latency_;
    }

    bool CircularBuffer::empty() const {
        return count_ == 0;
    }

    std::size_t CircularBuffer::size() const {
        return count_;
    }
}
