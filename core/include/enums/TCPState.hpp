#pragma once

namespace kns {
    enum class TCPState {
        CLOSED,
        SYN_SENT,
        SYN_RECEIVED,
        ESTABLISHED
    };
}