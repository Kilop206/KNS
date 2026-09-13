#pragma once

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

namespace kns::app {

[[nodiscard]] inline std::optional<std::string> readEnvironmentVariable(
    const char* name
)
{
#ifdef _WIN32
    char* raw_value = nullptr;
    std::size_t value_size = 0;
    if (_dupenv_s(&raw_value, &value_size, name) != 0) {
        return std::nullopt;
    }

    const std::unique_ptr<char, decltype(&std::free)> value(
        raw_value,
        &std::free
    );
    if (value == nullptr || value_size <= 1) {
        return std::nullopt;
    }

    return std::string(value.get());
#else
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }

    return std::string(value);
#endif
}

} // namespace kns::app
