#pragma once

#include <cstdint>
#include <optional>

namespace kns {
    class SimulationEngine;
}

namespace gui {

    enum class TcpConnectionActionType {
        Open,
        Cancel
    };

    struct TcpConnectionAction {
        TcpConnectionActionType type = TcpConnectionActionType::Open;
        int source = 0;
        int destination = 0;
        std::uint16_t source_port = 0;
        std::uint16_t destination_port = 0;
        std::uint64_t session_id = 0;
    };

    class TcpConnectionPanel {
    public:
        [[nodiscard]] std::optional<TcpConnectionAction> render(
            const kns::SimulationEngine& engine
        );

    private:
        int source_node_ = 0;
        int destination_node_ = 1;
        int source_port_ = 49152;
        int destination_port_ = 0;
    };

} // namespace gui
