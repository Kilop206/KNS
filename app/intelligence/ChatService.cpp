#include "intelligence/ChatService.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>
#include "intelligence/IntelligenceRequestBuilder.hpp"

namespace kns::app::intelligence {

ChatService::ChatService(Transport transport) : transport_(std::move(transport)) {}

void ChatService::synchronizeTopology(std::uint64_t revision)
{
    if (revision_ != revision) {
        revision_ = revision;
        clear();
    }
}

void ChatService::clear()
{
    ++generation_;
    messages_.clear();
    error_.clear();
    failed_request_.reset();
    // Keep polling an outstanding worker; clearing a std::async future would block.
}

void ChatService::send(const kns::analysis::NetworkAnalysis& analysis, std::string question)
{
    if (busy() || failed_request_) return;
    const auto begin = question.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return;
    question = question.substr(begin, question.find_last_not_of(" \t\r\n") - begin + 1);
    if (question.size() > 4096) {
        error_ = "Question exceeds 4096 UTF-8 bytes.";
        return;
    }
    try {
        auto snapshot = kns::intelligence::IntelligenceRequestBuilder::toJson(
            kns::intelligence::IntelligenceRequestBuilder::build(analysis, kns::intelligence::AnalysisMode::Deep));
        // Include complete identities, including nodes outside the critical lists.
        if (analysis.nodes.size() == analysis.node_count) {
            auto ids = nlohmann::json::array();
            for (const auto& node : analysis.nodes) ids.push_back(node.node_id);
            snapshot["context"]["network"]["node_ids"] = std::move(ids);
        }
        if (analysis.links.size() == analysis.link_count) {
            auto links = nlohmann::json::array();
            for (const auto& link : analysis.links) {
                links.push_back({{"from", link.from}, {"to", link.to},
                                 {"delay_ms", link.delay_ms}, {"bandwidth_mbps", link.bandwidth_mbps}});
            }
            snapshot["context"]["network"]["links"] = std::move(links);
        }
        auto history = nlohmann::json::array();
        // Bound both number and bytes, dropping only complete oldest turns.
        std::size_t start = messages_.size() > 20 ? messages_.size() - 20 : 0;
        std::size_t bytes = question.size();
        for (std::size_t i = start; i < messages_.size(); ++i) bytes += messages_[i].content.size();
        while (bytes > 64000 && start + 1 < messages_.size()) {
            bytes -= messages_[start].content.size() + messages_[start + 1].content.size();
            start += 2;
        }
        for (std::size_t i = start; i < messages_.size(); ++i) {
            history.push_back({{"role", messages_[i].role}, {"content", messages_[i].content}});
        }
        history.push_back({{"role", "user"}, {"content", question}});
        nlohmann::json request = {
            {"requestId", snapshot.at("request_id")}, {"topologyRevision", std::to_string(revision_)},
            {"analysisMode", snapshot.at("analysis_mode")}, {"context", snapshot.at("context")},
            {"score", snapshot.at("score")}, {"messages", std::move(history)}
        };
        messages_.push_back({"user", std::move(question)});
        launch(std::move(request));
    }
    catch (const std::exception& exception) { error_ = exception.what(); }
}

void ChatService::launch(nlohmann::json request)
{
    failed_request_ = request;
    error_.clear();
    active_generation_ = generation_;
    try {
        future_ = std::async(std::launch::async, [transport = transport_, request = std::move(request)] {
            return transport(request);
        });
    }
    catch (const std::exception& exception) { error_ = exception.what(); }
}

void ChatService::retry()
{
    if (canRetry()) launch(*failed_request_);
}

void ChatService::discardFailedQuestion()
{
    if (!canRetry()) return;
    messages_.pop_back();
    failed_request_.reset();
    error_.clear();
}

void ChatService::update()
{
    if (!busy() || future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto response = future_.get();
        if (active_generation_ != generation_) return;
        messages_.push_back({"assistant", std::move(response)});
        failed_request_.reset();
        error_.clear();
    }
    catch (const std::exception& exception) {
        if (active_generation_ == generation_) error_ = exception.what();
    }
    catch (...) {
        if (active_generation_ == generation_) error_ = "Unexpected KiWi chat error.";
    }
}

}
