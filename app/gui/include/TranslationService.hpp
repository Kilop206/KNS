#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace gui {

enum class UiLanguage {
    English,
    Portuguese,
    Spanish,
    French,
    German,
    Japanese,
    ChineseSimplified
};

struct UiLanguageOption {
    UiLanguage language;
    std::string_view display_name;
};

class TranslationService {
public:
    TranslationService();
    ~TranslationService();

    TranslationService(const TranslationService&) = delete;
    TranslationService& operator=(const TranslationService&) = delete;
    TranslationService(TranslationService&&) = delete;
    TranslationService& operator=(TranslationService&&) = delete;

    void setLanguage(UiLanguage language);
    [[nodiscard]] UiLanguage getLanguage() const noexcept;

    /// Returns a cached translation or the original English text while a
    /// request is pending/unavailable. Only strings explicitly passed here are
    /// sent to the configured translation service.
    [[nodiscard]] std::string translate(std::string_view english_text);

    [[nodiscard]] bool isTranslating() const noexcept;
    [[nodiscard]] std::string getLastError() const;
    void retry();

    [[nodiscard]] static std::span<const UiLanguageOption>
    languageOptions() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gui
