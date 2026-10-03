#include "gui/include/TranslationService.hpp"
#include "include/Environment.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <set>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace gui {
namespace {

using namespace std::chrono_literals;

constexpr std::array<UiLanguageOption, 7> kLanguages{{
    {UiLanguage::English, "English"},
    {UiLanguage::Portuguese, "Português"},
    {UiLanguage::Spanish, "Español"},
    {UiLanguage::French, "Français"},
    {UiLanguage::German, "Deutsch"},
    {UiLanguage::Japanese, "日本語"},
    {UiLanguage::ChineseSimplified, "简体中文"},
}};

std::string_view apiLanguage(UiLanguage language)
{
    switch (language) {
        case UiLanguage::English: return "english";
        case UiLanguage::Portuguese: return "portuguese";
        case UiLanguage::Spanish: return "spanish";
        case UiLanguage::French: return "french";
        case UiLanguage::German: return "german";
        case UiLanguage::Japanese: return "japanese";
        case UiLanguage::ChineseSimplified: return "chinese_simplified";
    }

    return "english";
}

std::string apiBaseUrl()
{
    if (const auto configured =
            kns::app::readEnvironmentVariable("KNS_TRANSLATION_API_BASE_URL")) {
        std::string result(*configured);
        while (!result.empty() && result.back() == '/') {
            result.pop_back();
        }
        return result;
    }

    return "http://api.translate.zvo.cn";
}

std::vector<std::string> requestTranslations(
    const std::string& base_url,
    UiLanguage language,
    const std::vector<std::string>& source
)
{
    httplib::Client client(base_url);
    client.set_connection_timeout(3, 0);
    client.set_read_timeout(8, 0);
    client.set_write_timeout(3, 0);

    const httplib::Params parameters{
        {"to", std::string(apiLanguage(language))},
        {"text", nlohmann::json(source).dump()},
    };

    const auto response = client.Post("/translate.json", parameters);
    if (!response) {
        throw std::runtime_error(
            "translation request failed: " + httplib::to_string(response.error())
        );
    }
    if (response->status != 200) {
        throw std::runtime_error(
            "translation service returned HTTP " +
            std::to_string(response->status)
        );
    }

    const auto payload = nlohmann::json::parse(response->body);
    if (payload.value("result", 0) != 1 || !payload.contains("text")) {
        throw std::runtime_error(
            payload.value("info", "translation service returned an invalid response")
        );
    }

    auto translated = payload.at("text").get<std::vector<std::string>>();
    if (translated.size() != source.size()) {
        throw std::runtime_error("translation service returned an incomplete batch");
    }

    return translated;
}

} // namespace

class TranslationService::Impl {
public:
    Impl()
        : base_url_(apiBaseUrl()),
          worker_([this](std::stop_token stop_token) { run(stop_token); })
    {
    }

    ~Impl()
    {
        worker_.request_stop();
        condition_.notify_all();
    }

    void setLanguage(UiLanguage language)
    {
        std::scoped_lock lock(mutex_);
        if (language_ == language) {
            return;
        }

        language_ = language;
        ++generation_;
        cache_.clear();
        pending_.clear();
        last_error_.clear();
        retry_after_ = {};
        failure_count_ = 0;
        condition_.notify_all();
    }

    UiLanguage getLanguage() const noexcept
    {
        std::scoped_lock lock(mutex_);
        return language_;
    }

    std::string translate(std::string_view english_text)
    {
        if (english_text.empty()) {
            return {};
        }

        std::scoped_lock lock(mutex_);
        if (language_ == UiLanguage::English) {
            return std::string(english_text);
        }

        const std::string source(english_text);
        if (const auto cached = cache_.find(source); cached != cache_.end()) {
            return cached->second;
        }

        const bool inserted = pending_.insert(source).second;
        if (inserted) {
            condition_.notify_one();
        }

        return source;
    }

    bool isTranslating() const noexcept
    {
        std::scoped_lock lock(mutex_);
        return request_in_flight_ || !pending_.empty();
    }

    std::string getLastError() const
    {
        std::scoped_lock lock(mutex_);
        return last_error_;
    }

    void retry()
    {
        std::scoped_lock lock(mutex_);
        last_error_.clear();
        retry_after_ = std::chrono::steady_clock::now();
        condition_.notify_one();
    }

private:
    void run(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested()) {
            std::vector<std::string> source;
            UiLanguage language = UiLanguage::English;
            std::size_t generation = 0;

            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock, [&] {
                    return stop_token.stop_requested() ||
                        (language_ != UiLanguage::English && !pending_.empty());
                });
                if (stop_token.stop_requested()) {
                    return;
                }

                const std::size_t observed_generation = generation_;
                if (std::chrono::steady_clock::now() < retry_after_) {
                    condition_.wait_until(
                        lock,
                        retry_after_,
                        [&] {
                            return stop_token.stop_requested() ||
                                generation_ != observed_generation;
                        }
                    );
                    if (stop_token.stop_requested()) {
                        return;
                    }
                    if (generation_ != observed_generation) {
                        continue;
                    }
                }

                // Collect all labels produced by one UI frame. Notifications
                // for additional labels do not shorten this batching window.
                const auto batch_deadline =
                    std::chrono::steady_clock::now() + 25ms;
                condition_.wait_until(
                    lock,
                    batch_deadline,
                    [&] {
                        return stop_token.stop_requested() ||
                            generation_ != observed_generation ||
                            language_ == UiLanguage::English;
                    }
                );
                if (stop_token.stop_requested()) {
                    return;
                }
                if (generation_ != observed_generation ||
                    language_ == UiLanguage::English) {
                    continue;
                }

                source.assign(pending_.begin(), pending_.end());
                pending_.clear();
                language = language_;
                generation = generation_;
                request_in_flight_ = true;
            }

            try {
                auto translated = requestTranslations(base_url_, language, source);
                std::scoped_lock lock(mutex_);
                if (generation == generation_ && language == language_) {
                    for (std::size_t i = 0; i < source.size(); ++i) {
                        cache_.insert_or_assign(source[i], std::move(translated[i]));
                    }
                    last_error_.clear();
                    failure_count_ = 0;
                    retry_after_ = {};
                }
            } catch (const std::exception& error) {
                std::scoped_lock lock(mutex_);
                if (generation == generation_) {
                    last_error_ = error.what();
                    pending_.insert(source.begin(), source.end());
                    ++failure_count_;

                    const auto exponent = std::min<std::size_t>(
                        failure_count_ - 1,
                        5
                    );
                    const auto retry_delay = std::min(
                        std::chrono::seconds{1 << exponent},
                        30s
                    );
                    retry_after_ =
                        std::chrono::steady_clock::now() + retry_delay;
                    condition_.notify_one();
                }
            }

            {
                std::scoped_lock lock(mutex_);
                request_in_flight_ = false;
            }
        }
    }

    const std::string base_url_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    UiLanguage language_ = UiLanguage::English;
    std::size_t generation_ = 0;
    std::unordered_map<std::string, std::string> cache_;
    std::set<std::string> pending_;
    std::string last_error_;
    std::chrono::steady_clock::time_point retry_after_{};
    std::size_t failure_count_ = 0;
    bool request_in_flight_ = false;
    std::jthread worker_;
};

TranslationService::TranslationService()
    : impl_(std::make_unique<Impl>())
{
}

TranslationService::~TranslationService() = default;

void TranslationService::setLanguage(UiLanguage language)
{
    impl_->setLanguage(language);
}

UiLanguage TranslationService::getLanguage() const noexcept
{
    return impl_->getLanguage();
}

std::string TranslationService::translate(std::string_view english_text)
{
    return impl_->translate(english_text);
}

std::string TranslationService::label(
    std::string_view english_text,
    std::string_view stable_id
)
{
    std::string result = translate(english_text);
    result += "###";
    result += stable_id;
    return result;
}

bool TranslationService::isTranslating() const noexcept
{
    return impl_->isTranslating();
}

std::string TranslationService::getLastError() const
{
    return impl_->getLastError();
}

void TranslationService::retry()
{
    impl_->retry();
}

std::span<const UiLanguageOption> TranslationService::languageOptions() noexcept
{
    return kLanguages;
}

} // namespace gui
