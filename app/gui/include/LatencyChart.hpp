#pragma once

#include <cstddef>
#include <vector>

namespace interface {
    struct CircularBuffer {
    public:
        explicit CircularBuffer(std::size_t capacity = 100);

        void addLatencyToBuffer(float latency);

        std::vector<float> values() const;

        float getAverageLatency() const;
        float getMinimumLatency() const;
        float getMaximumLatency() const;

        bool empty() const;
        std::size_t size() const;

    private:
        std::size_t capacity_;
        std::vector<float> buffer_;
        std::size_t start_ = 0;
        std::size_t count_ = 0;

        float maximum_latency_ = 0.0f;
        float avg_latency_ = 0.0f;
        float minimum_latency_ = 0.0f;

        void recomputeStatistics();
    };
}
