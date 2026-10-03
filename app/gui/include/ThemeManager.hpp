#pragma once

#include "imgui.h"

namespace gui {

    class ThemeManager {
        public:
            static void apply(const char* fontPathRegular,
                            const char* fontPathBold = nullptr,
                            const char* fontPathMono = nullptr);

            static ImFont* regular();
            static ImFont* bold();
            static ImFont* mono();

        private:
            static ImFont* loadFont(ImGuiIO& io, const char* path, float size);

            static ImFont* regular_;
            static ImFont* bold_;
            static ImFont* mono_;
    };

}