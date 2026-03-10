#pragma once

#include <cstdint>

namespace core {

    class SimulationEngine;

    class Event {
    public:
        virtual ~Event() = default;

        virtual void execute(SimulationEngine& engine) = 0;

        std::uint64_t getTimestamp() const noexcept;
        std::uint64_t getId() const noexcept;

    protected:
        Event(std::uint64_t timestamp);

    private:
        std::uint64_t timestamp_;
        std::uint64_t id_;
        static std::uint64_t nextId_;
    };
}