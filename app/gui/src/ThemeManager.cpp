#include "gui/include/ThemeManager.hpp"

#include <cstdio>
#include <array>
#include <filesystem>
#include <span>
#include <string_view>

namespace {

std::filesystem::path findFirstExistingFont(
    std::span<const std::string_view> candidates
)
{
    std::error_code ec;
    for (const std::string_view candidate : candidates) {
        const std::filesystem::path path(candidate);
        if (std::filesystem::exists(path, ec)) {
            return path;
        }
        ec.clear();
    }

    return {};
}

void mergeSystemFont(
    ImGuiIO& io,
    std::span<const std::string_view> candidates,
    const ImWchar* glyph_ranges,
    float size
)
{
    const std::filesystem::path path = findFirstExistingFont(candidates);
    if (path.empty()) {
        return;
    }

    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    config.OversampleH = 1;
    config.OversampleV = 1;

    if (io.Fonts->AddFontFromFileTTF(
            path.string().c_str(),
            size,
            &config,
            glyph_ranges
        ) == nullptr) {
        std::fprintf(
            stderr,
            "Failed to load CJK fallback font: %s\n",
            path.string().c_str()
        );
    }
}

void mergeCjkFallbacks(ImGuiIO& io, float size)
{
#ifdef _WIN32
    constexpr std::array<std::string_view, 3> japanese_fonts{
        "C:/Windows/Fonts/YuGothM.ttc",
        "C:/Windows/Fonts/meiryo.ttc",
        "C:/Windows/Fonts/msgothic.ttc",
    };
    constexpr std::array<std::string_view, 3> chinese_fonts{
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyhbd.ttc",
        "C:/Windows/Fonts/simhei.ttf",
    };
#elif defined(__APPLE__)
    constexpr std::array<std::string_view, 2> japanese_fonts{
        "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc",
        "/System/Library/Fonts/AppleSDGothicNeo.ttc",
    };
    constexpr std::array<std::string_view, 2> chinese_fonts{
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
    };
#else
    constexpr std::array<std::string_view, 3> japanese_fonts{
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKjp-Regular.otf",
        "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
    };
    constexpr std::array<std::string_view, 3> chinese_fonts{
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
    };
#endif

    mergeSystemFont(
        io,
        japanese_fonts,
        io.Fonts->GetGlyphRangesJapanese(),
        size
    );
    mergeSystemFont(
        io,
        chinese_fonts,
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon(),
        size
    );
}

} // namespace

namespace gui {

    ImFont* ThemeManager::regular_ = nullptr;
    ImFont* ThemeManager::bold_ = nullptr;
    ImFont* ThemeManager::mono_ = nullptr;

    void ThemeManager::apply(const char* fontPathRegular,
                            const char* fontPathBold,
                            const char* fontPathMono)
    {
        ImGui::StyleColorsLight();

        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_WindowBg]       = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ChildBg]        = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_PopupBg]        = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_Text]           = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]        = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
        style.Colors[ImGuiCol_TitleBg]        = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg]      = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_Border]         = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        regular_ = loadFont(io, fontPathRegular, 18.0f);
        if (!regular_) {
            regular_ = io.Fonts->AddFontDefault();
        }

        mergeCjkFallbacks(io, 18.0f);

        bold_ = loadFont(io, fontPathBold, 18.0f);
        if (!bold_) {
            bold_ = regular_;
        }

        mono_ = loadFont(io, fontPathMono, 17.0f);
        if (!mono_) {
            mono_ = regular_;
        }

        io.FontDefault = regular_;
    }

    ImFont* ThemeManager::loadFont(ImGuiIO& io, const char* path, float size) {
        if (!path) {
            return nullptr;
        }

        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            std::fprintf(stderr, "Font file not found: %s\n", path);
            return nullptr;
        }

        ImFont* font = io.Fonts->AddFontFromFileTTF(path, size);
        if (!font) {
            std::fprintf(stderr, "Failed to load font: %s\n", path);
        }

        return font;
    }

    ImFont* ThemeManager::regular() {
        return regular_;
    }

    ImFont* ThemeManager::bold() {
        return bold_;
    }

    ImFont* ThemeManager::mono() {
        return mono_;
    }

}
