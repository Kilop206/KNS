#pragma once

namespace kns {

    struct Stats {
        int packets_sent = 0;
        int packets_delivered = 0;
        int packets_lost = 0;
        int data_packets_delivered = 0;
        int queue_overflow_drops = 0;
        int red_early_drops = 0;

        double total_latency = 0.0;
    };

}
