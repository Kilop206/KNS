#include "network/transport/tcp/buffer/TCPReceiveBuffer.hpp"

#include <algorithm>
#include <utility>

namespace kns {

    bool TCPReceiveBuffer::push(TCPReceiveEntry entry)
    {
        if (entry.payload_size() == 0) {
            return false;
        }

        const auto offset = tcp_sequence::distance(next_sequence_, entry.segment.seq);
        if (entry.payload_size() >= tcp_sequence::half_space ||
            offset >= tcp_sequence::half_space - entry.payload_size()) {
            return false;
        }

        if (
            capacity_bytes_ != 0 &&
            entry.payload_size() > availableWindow()
        ) {
            return false;
        }

        const auto duplicate = std::find_if(
            entries_.begin(),
            entries_.end(),
            [this, offset, &entry](const TCPReceiveEntry& existing) {
                const auto start = tcp_sequence::distance(next_sequence_, existing.segment.seq);
                return offset < start + existing.payload_size() &&
                    start < offset + entry.payload_size();
            }
        );

        if (duplicate != entries_.end()) {
            return false;
        }

        const auto position = std::lower_bound(
            entries_.begin(),
            entries_.end(),
            entry.segment.seq,
            [this](const TCPReceiveEntry& existing,
               std::uint32_t seq) {
                return tcp_sequence::distance(next_sequence_, existing.segment.seq) <
                    tcp_sequence::distance(next_sequence_, seq);
            }
        );

        const auto bytes = entry.payload_size();
        entries_.insert(
            position,
            std::move(entry)
        );
        buffered_bytes_ += bytes;

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

            next_sequence_ =
                entry.sequence_end();

            buffered_bytes_ -=
                entry.payload_size();

            entries_.pop_front();

            ++consumed;
        }

        return consumed;
    }

    void TCPReceiveBuffer::clear() noexcept
    {
        entries_.clear();
        buffered_bytes_ = 0;
    }

} // namespace kns
