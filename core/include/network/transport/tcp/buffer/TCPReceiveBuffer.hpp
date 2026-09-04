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
            std::size_t capacity_bytes = 0
        ) noexcept
            : next_sequence_(next_sequence),
              capacity_bytes_(capacity_bytes),
              buffered_bytes_(0)
        {
        }

        bool push(TCPReceiveEntry entry);

        std::size_t size() const noexcept {
            return entries_.size();
        }

        bool empty() const noexcept {
            return entries_.empty();
        }

        std::size_t capacity() const noexcept {
            return capacity_bytes_;
        }

        std::size_t bufferedBytes() const noexcept {
            return buffered_bytes_;
        }

        std::size_t availableWindow() const noexcept {
            if (capacity_bytes_ == 0) {
                return 0;
            }

            if (buffered_bytes_ >= capacity_bytes_) {
                return 0;
            }

            return capacity_bytes_ - buffered_bytes_;
        }

        void setCapacity(
            std::size_t capacity_bytes
        ) noexcept
        {
            capacity_bytes_ = capacity_bytes;
        }

        std::uint32_t nextSequence() const noexcept {
            return next_sequence_;
        }

        void setNextSequence(
            std::uint32_t sequence
        ) noexcept
        {
            next_sequence_ = sequence;
        }

        const TCPReceiveEntry* front() const noexcept {
            if (entries_.empty()) {
                return nullptr;
            }

            return &entries_.front();
        }

        std::size_t consumeContiguous();

        void clear() noexcept;

    private:
        std::deque<TCPReceiveEntry> entries_;

        std::uint32_t next_sequence_;

        std::size_t capacity_bytes_;
        std::size_t buffered_bytes_;
    };

} // namespace kns