#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

#include "network/transport/tcp/buffer/TCPSendEntry.hpp"

namespace kns {

    class TCPSendBuffer {
    public:
        explicit TCPSendBuffer(std::size_t capacity = 0) noexcept
            : capacity_(capacity) {}

        bool push(TCPSendEntry entry);

        bool empty() const noexcept {
            return entries_.empty();
        }

        std::size_t size() const noexcept {
            return entries_.size();
        }

        std::size_t capacity() const noexcept {
            return capacity_;
        }

        void setCapacity(std::size_t capacity) noexcept {
            capacity_ = capacity;
        }

        const TCPSendEntry* front() const noexcept {
            if (entries_.empty()) {
                return nullptr;
            }

            return &entries_.front();
        }

        TCPSendEntry* front() noexcept {
            if (entries_.empty()) {
                return nullptr;
            }

            return &entries_.front();
        }

        std::size_t acknowledge(std::uint32_t ack_number);

        std::optional<TCPSendEntry> popFront();

        void clear() noexcept {
            entries_.clear();
        }

    private:
        std::deque<TCPSendEntry> entries_;
        std::size_t capacity_;
    };

} // namespace kns