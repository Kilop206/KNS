#pragma once

#include <cstddef>
#include <functional>

namespace kns {

    struct TCPConnectionKey {
        int source = 0;
        int destination = 0;

        constexpr TCPConnectionKey() = default;

        constexpr TCPConnectionKey(int src, int dst)
            : source(src), destination(dst) {}

        constexpr bool operator==(const TCPConnectionKey& other) const noexcept {
            return source == other.source &&
                destination == other.destination;
        }
    };

}

namespace std {
    template <>
    struct hash<kns::TCPConnectionKey> {
        std::size_t operator()(const kns::TCPConnectionKey& key) const noexcept {
            const std::size_t h1 = std::hash<int>{}(key.source);
            const std::size_t h2 = std::hash<int>{}(key.destination);
            return h1 ^ (h2 << 1);
        }
    };
}