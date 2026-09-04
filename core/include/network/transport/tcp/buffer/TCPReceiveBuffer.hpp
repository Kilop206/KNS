#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include "network/transport/tcp/buffer/TCPReceiveEntry.hpp"

namespace kns {

    class TCPReceiveBuffer {
    public:
        explicit TCPReceiveBuffer(
            std::uint32_t next_sequence = 0,
            std::size_t capacity = 0
        ) noexcept
            : next_sequence_(next_sequence),
              capacity_(capacity) {}

        bool push(TCPReceiveEntry entry);

        std::size_t size() const noexcept {
            return entries_.size();
        }

        bool empty() const noexcept {
            return entries_.empty();
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        void setCapacity(std::size_t capacity) noexcept {
            capacity_ = capacity;
        }

        std::uint32_t nextSequence() const noexcept {
            return next_sequence_;
        }

        void setNextSequence(std::uint32_t sequence) noexcept {
            next_sequence_ = sequence;
        }

        std::size_t availableCapacity() const noexcept {
            if (capacity_ == 0) {
                return 0;
            }

            return capacity_ > entries_.size()
                ? capacity_ - entries_.size()
                : 0;
        }

        const TCPReceiveEntry* front() const noexcept {
            if (entries_.empty()) {
                return nullptr;
            }

            return &entries_.front();
        }

        std::size_t consumeContiguous();

        void clear() noexcept {
            entries_.clear();
        }

    private:
        std::deque<TCPReceiveEntry> entries_;
        std::uint32_t next_sequence_;
        std::size_t capacity_;
    };

} // namespace kns