#pragma once

#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "analysis/NetworkAnalysis.hpp"

namespace kns::app::intelligence {

struct ChatMessage {
    std::string role;
    std::string content;
};

// Owned by the UI thread. Workers receive only copied request/transport values.
class ChatService {
public:
    using Transport = std::function<std::string(const nlohmann::json&)>;
    explicit ChatService(Transport transport);
    void synchronizeTopology(std::uint64_t revision);
    void clear();
    void send(const kns::analysis::NetworkAnalysis& analysis, std::string question);
    void retry();
    void discardFailedQuestion();
    void update();
    [[nodiscard]] bool busy() const { return future_.valid(); }
    [[nodiscard]] bool canRetry() const { return !busy() && failed_request_.has_value(); }
    [[nodiscard]] const std::vector<ChatMessage>& messages() const { return messages_; }
    [[nodiscard]] const std::string& error() const { return error_; }

private:
    void launch(nlohmann::json request);
    Transport transport_;
    std::future<std::string> future_;
    std::vector<ChatMessage> messages_;
    std::optional<nlohmann::json> failed_request_;
    std::uint64_t revision_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t active_generation_ = 0;
    std::string error_;
};

}
