#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace kns::app::intelligence {

struct ChatReply {
    std::string message;
    std::size_t history_turns_omitted = 0;
    std::string plan;
    std::optional<int> daily_quota_remaining;
};

}
