#pragma once

#include <cstdint>
#include <vector>

namespace kns {

    enum class TCPFlag : std::uint8_t {
        None = 0,
        SYN  = 1 << 0,
        ACK  = 1 << 1,
        FIN  = 1 << 2,
        RST  = 1 << 3,
        PSH  = 1 << 4
    };

    constexpr TCPFlag operator|(TCPFlag a, TCPFlag b) {
        return static_cast<TCPFlag>(
            static_cast<std::uint8_t>(a) |
            static_cast<std::uint8_t>(b)
        );
    }

    constexpr TCPFlag operator&(TCPFlag a, TCPFlag b) {
        return static_cast<TCPFlag>(
            static_cast<std::uint8_t>(a) &
            static_cast<std::uint8_t>(b)
        );
    }

    constexpr TCPFlag operator~(TCPFlag a) {
        return static_cast<TCPFlag>(
            ~static_cast<std::uint8_t>(a)
        );
    }

    inline bool hasFlag(TCPFlag flags, TCPFlag flag) {
        return (static_cast<std::uint8_t>(flags) &
                static_cast<std::uint8_t>(flag)) != 0;
    }

    struct TCPSegment {
        std::uint32_t seq = 0;
        std::uint32_t ack = 0;
        std::uint16_t window = 0;

        TCPFlag flags = TCPFlag::None;
        std::vector<std::uint8_t> payload;

        bool syn() const noexcept { return hasFlag(flags, TCPFlag::SYN); }
        bool ackFlag() const noexcept { return hasFlag(flags, TCPFlag::ACK); }
        bool fin() const noexcept { return hasFlag(flags, TCPFlag::FIN); }
        bool rst() const noexcept { return hasFlag(flags, TCPFlag::RST); }
        bool psh() const noexcept { return hasFlag(flags, TCPFlag::PSH); }

        void setSyn(bool v) noexcept { setFlag(TCPFlag::SYN, v); }
        void setAck(bool v) noexcept { setFlag(TCPFlag::ACK, v); }
        void setFin(bool v) noexcept { setFlag(TCPFlag::FIN, v); }
        void setRst(bool v) noexcept { setFlag(TCPFlag::RST, v); }
        void setPsh(bool v) noexcept { setFlag(TCPFlag::PSH, v); }

        std::size_t payloadSize() const noexcept { return payload.size(); }

    private:
        void setFlag(TCPFlag flag, bool value) noexcept {
            if (value) {
                flags = flags | flag;
            } else {
                flags = static_cast<TCPFlag>(
                    static_cast<std::uint8_t>(flags) &
                    ~static_cast<std::uint8_t>(flag)
                );
            }
        }
    };
}