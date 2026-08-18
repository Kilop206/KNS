#include "gui/include/ThemeManager.hpp"

#include <cstdio>
#include <filesystem>

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