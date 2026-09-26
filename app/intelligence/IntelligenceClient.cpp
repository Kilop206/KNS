#include "intelligence/IntelligenceClient.hpp"

#include <httplib.h>

#include <stdexcept>
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

} // namespace

IntelligenceClient::IntelligenceClient(
    IntelligenceClientConfig config
)
    : config_(std::move(config))
{
}

std::string IntelligenceClient::chat(const nlohmann::json& request) const
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
    return message;
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
