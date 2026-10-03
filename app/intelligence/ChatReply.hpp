#pragma once

#include <cstddef>
#include <string>

namespace kns::app::intelligence {

struct ChatReply {
    std::string message;
    std::size_t history_turns_omitted = 0;
};

}
