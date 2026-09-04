#include "network/transport/tcp/buffer/TCPReceiveBuffer.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace kns {

    bool TCPReceiveBuffer::push(TCPReceiveEntry entry)
    {
        if (capacity_ != 0 && entries_.size() >= capacity_) {
            return false;
        }

        if (entry.payload_size() == 0) {
            return false;
        }

        if (entry.segment.seq < next_sequence_) {
            return false;
        }

        const auto duplicate = std::find_if(
            entries_.begin(),
            entries_.end(),
            [&entry](const TCPReceiveEntry& existing) {
                return existing.segment.seq == entry.segment.seq;
            }
        );

        if (duplicate != entries_.end()) {
            return false;
        }

        const auto position = std::lower_bound(
            entries_.begin(),
            entries_.end(),
            entry.segment.seq,
            [](const TCPReceiveEntry& existing, std::uint32_t seq) {
                return existing.segment.seq < seq;
            }
        );

        entries_.insert(position, std::move(entry));

        return true;
    }

    std::size_t TCPReceiveBuffer::consumeContiguous()
    {
        std::size_t consumed = 0;

        while (!entries_.empty()) {
            TCPReceiveEntry& entry = entries_.front();

            if (entry.segment.seq != next_sequence_) {
                break;
            }

            next_sequence_ = entry.sequence_end();

            entries_.pop_front();
            ++consumed;
        }

        return consumed;
    }

} // namespace kns