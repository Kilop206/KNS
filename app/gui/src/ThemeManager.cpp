#include "gui/include/ThemeManager.hpp"

#include <stdexcept>

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

        if (fontPathRegular) {
            regular_ = io.Fonts->AddFontFromFileTTF(fontPathRegular, 18.0f);
            if (!regular_) {
                throw std::runtime_error(std::string("Failed to load font: ") + fontPathRegular);
            }
        }

        if (fontPathBold) {
            bold_ = io.Fonts->AddFontFromFileTTF(fontPathBold, 18.0f);
            if (!bold_) {
                throw std::runtime_error(std::string("Failed to load font: ") + fontPathBold);
            }
        } else {
            bold_ = regular_;
        }

        if (fontPathMono) {
            mono_ = io.Fonts->AddFontFromFileTTF(fontPathMono, 17.0f);
            if (!mono_) {
                throw std::runtime_error(std::string("Failed to load font: ") + fontPathMono);
            }
        } else {
            mono_ = regular_;
        }

        io.FontDefault = regular_;
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