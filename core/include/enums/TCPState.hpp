#pragma once

namespace kns {
    enum class TCPState {
        CLOSED,
<<<<<<< HEAD
        SYN_SENT,
        SYN_RECEIVED,
        ESTABLISHED
=======
        LISTEN,
        SYN_SENT,
        SYN_RECEIVED,
        ESTABLISHED,
        FIN_WAIT_1,
        FIN_WAIT_2,
        CLOSE_WAIT,
        CLOSING,
        LAST_ACK,
        TIME_WAIT,
>>>>>>> 879e9a30eb706359e007b3218a4c881c257cd5bc
    };
}