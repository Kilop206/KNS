#pragma once

#include <string>

#include "engine/core/Event.hpp"
#include "engine/core/SimulationEngine.hpp"

namespace kns {
    class PrintEvent : public kns::Event {
    private:
        int time;
        std::string message;

    public:
        PrintEvent(int time, const std::string& message);

        std::uint64_t getTimestamp() const noexcept;

        void execute(kns::SimulationEngine& engine) override;
        const char* getName() const noexcept override { return "PrintEvent"; }
    };
}
