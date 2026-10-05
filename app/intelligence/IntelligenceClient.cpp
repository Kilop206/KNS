#include "intelligence/IntelligenceClient.hpp"

#include <httplib.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string_view>
#include <string>
#include <utility>

#include "intelligence/IntelligenceRequestBuilder.hpp"
#include "intelligence/IntelligenceResponseParser.hpp"

namespace kns::app::intelligence {

namespace {

std::string httpErrorMessage(
    int status,
    const std::string& body
)
{
    std::string message =
        "Intelligence backend returned HTTP " +
        std::to_string(status);

    if (!body.empty()) {
        message += ": " + body;
    }

    return message;
}


bool validRequestId(std::string_view value) noexcept
{
    return !value.empty() &&
        value.size() <= 128 &&
        std::all_of(
            value.begin(),
            value.end(),
            [](unsigned char character) {
                return std::isalnum(character) ||
                    character == '-' ||
                    character == '_' ||
                    character == '.';
            }
        );
}

void addRequestIdHeader(
    httplib::Headers& headers,
    const nlohmann::json& request,
    const char* key
)
{
    const auto iterator = request.find(key);
    if (iterator == request.end() || !iterator->is_string()) {
        return;
    }
    const auto value = iterator->get<std::string>();
    if (validRequestId(value)) {
        headers.emplace("X-Request-ID", value);
    }
}

} // namespace

IntelligenceClient::IntelligenceClient(
    IntelligenceClientConfig config
)
    : config_(std::move(config))
{
}

ChatReply IntelligenceClient::chat(const nlohmann::json& request) const
{
    httplib::Client client(config_.base_url);
    client.set_connection_timeout(config_.connection_timeout_seconds);
    client.set_read_timeout(config_.chat_timeout_seconds);
    client.set_write_timeout(config_.write_timeout_seconds);
    client.set_payload_max_length(128 * 1024);
    httplib::Headers headers{{"Accept", "application/json"}, {"User-Agent", "KNS/1.0"}};
    if (!config_.bearer_token.empty()) {
        headers.emplace("Authorization", "Bearer " + config_.bearer_token);
    }
    addRequestIdHeader(headers, request, "requestId");
    const auto result = client.Post(config_.chat_endpoint, headers, request.dump(), "application/json");
    if (!result) {
        throw std::runtime_error("Could not reach KiWi chat: " + httplib::to_string(result.error()));
    }
    if (result->status < 200 || result->status >= 300) {
        throw std::runtime_error(httpErrorMessage(result->status, result->body.substr(0, 2048)));
    }
    const auto response = nlohmann::json::parse(result->body);
    if (response.at("requestId") != request.at("requestId") ||
        response.at("topologyRevision") != request.at("topologyRevision")) {
        throw std::runtime_error("KiWi returned a reply for a different request or topology");
    }
    auto message = response.at("message").get<std::string>();
    if (message.empty() || message.size() > 64000 ||
        message.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::runtime_error("KiWi returned an empty or oversized reply");
    }
    std::size_t omitted = 0;
    if (response.contains("historyTurnsOmitted")) {
        const auto& value = response.at("historyTurnsOmitted");
        if (!value.is_number_integer() || value < 0 ||
            value > request.at("messages").size() / 2) {
            throw std::runtime_error("KiWi returned invalid chat history metadata");
        }
        omitted = value.get<std::size_t>();
    }
    return {std::move(message), omitted};
}

kns::intelligence::IntelligenceResponse
IntelligenceClient::analyze(
    const kns::intelligence::IntelligenceRequest& request
) const
{
    httplib::Client client(
        config_.base_url
    );

    client.set_connection_timeout(
        config_.connection_timeout_seconds
    );

    client.set_read_timeout(
        config_.read_timeout_seconds
    );

    client.set_write_timeout(
        config_.write_timeout_seconds
    );

    httplib::Headers headers = {
        {
            "Accept",
            "application/json"
        },
        {
            "User-Agent",
            "KNS/1.0"
        }
    };

    if (!config_.bearer_token.empty()) {
        headers.emplace(
            "Authorization",
            "Bearer " + config_.bearer_token
        );
    }

    const auto requestJson =
        kns::intelligence::
            IntelligenceRequestBuilder::toJson(
                request
            );

    addRequestIdHeader(headers, requestJson, "request_id");

    const auto result =
        client.Post(
            config_.analyze_endpoint,
            headers,
            requestJson.dump(),
            "application/json"
        );

    if (!result) {
        throw std::runtime_error(
            "Could not connect to KNS Intelligence backend: " +
            httplib::to_string(result.error())
        );
    }

    if (
        result->status < 200 ||
        result->status >= 300
    ) {
        throw std::runtime_error(
            httpErrorMessage(
                result->status,
                result->body
            )
        );
    }

    return kns::intelligence::
        IntelligenceResponseParser::parse(
            result->body
        );
}

}
