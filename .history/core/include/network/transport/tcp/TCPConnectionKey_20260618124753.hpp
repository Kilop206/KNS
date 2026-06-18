#pragma once

namespace kns {

    struct TCPConnectionKey
    {
        int from;
        int to;

        bool operator==(const TCPConnectionKey& other) const
        {
            return from == other.from
                && to == other.to;
        }
    };

}