#include "network/transport/tcp/buffer/TCPSendBuffer.hpp"

namespace kns {

    bool TCPSendBuffer::push(TCPSendEntry entry)
    {
        if (capacity_ != 0 && entries_.size() >= capacity_) {
            return false;
        }

        entries_.push_back(std::move(entry));
        return true;
    }

    std::size_t TCPSendBuffer::acknowledge(std::uint32_t ack_number)
    {
        std::size_t removed = 0;

        while (!entries_.empty() &&
               entries_.front().isAcknowledged(ack_number)) {
            entries_.pop_front();
            ++removed;
        }

        return removed;
    }

    std::optional<TCPSendEntry> TCPSendBuffer::popFront()
    {
        if (entries_.empty()) {
            return std::nullopt;
        }

        TCPSendEntry entry = std::move(entries_.front());
        entries_.pop_front();

        return entry;
    }

} // namespace kns