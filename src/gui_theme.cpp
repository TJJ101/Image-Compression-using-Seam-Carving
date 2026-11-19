// Look and feel of the GUI (see gui_theme.hpp): ImGui style and colours, and the UI fonts.
#include "seamcarve/gui_theme.hpp"
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <system_error>

namespace seamcarve {
    namespace theme {
        namespace {
            /**
            * @brief Loads the first font file in the list that exists on this computer.
            * @param[in] candidates	- Font file paths to try, in order.
            * @return The loaded font, or nullptr if none of the files exist.
            */
            ImFont* LoadFirstFont(std::initializer_list<std::string> candidates) {
                for (const std::string& path : candidates) {
                    std::error_code ignored;  // unreadable counts as missing: try the next one
                    if (std::filesystem::exists(path, ignored)) {
                        return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str());
                    }
                }
                return nullptr;
            }
        }  // namespace

        void Apply(float displayScale) {
            ImGui::StyleColorsDark();
            ImGuiStyle& style{ ImGui::GetStyle() };

            style.WindowPadding = ImVec2{ 14.0f, 12.0f };
            style.FramePadding = ImVec2{ 10.0f, 6.0f };
            style.ItemSpacing = ImVec2{ 10.0f, 8.0f };
            style.ItemInnerSpacing = ImVec2{ 8.0f, 6.0f };
            style.ScrollbarSize = 12.0f;
            style.GrabMinSize = 12.0f;
            style.WindowBorderSize = 0.0f;
            style.FrameBorderSize = 0.0f;
            style.PopupBorderSize = 1.0f;
            style.TabBorderSize = 0.0f;
            style.WindowRounding = 8.0f;
            style.FrameRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding = 6.0f;
            style.TabRounding = 6.0f;
            style.SeparatorTextBorderSize = 1.0f;
            style.SeparatorTextPadding = ImVec2{ 0.0f, 6.0f };  // headings start at the left edge
            style.DockingSeparatorSize = 2.0f;

            ImVec4* const colors{ style.Colors };
            colors[ImGuiCol_Text] = text;
            colors[ImGuiCol_TextDisabled] = textMuted;
            colors[ImGuiCol_WindowBg] = background;
            colors[ImGuiCol_ChildBg] = background;
            colors[ImGuiCol_PopupBg] = surface;
            colors[ImGuiCol_Border] = border;
            colors[ImGuiCol_BorderShadow] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };
            colors[ImGuiCol_FrameBg] = surface;
            colors[ImGuiCol_FrameBgHovered] = surfaceHovered;
            colors[ImGuiCol_FrameBgActive] = surfaceActive;
            colors[ImGuiCol_TitleBg] = backgroundDark;
            colors[ImGuiCol_TitleBgActive] = backgroundDark;
            colors[ImGuiCol_TitleBgCollapsed] = backgroundDark;
            colors[ImGuiCol_MenuBarBg] = backgroundDark;
            colors[ImGuiCol_ScrollbarBg] = background;
            colors[ImGuiCol_ScrollbarGrab] = surfaceHovered;
            colors[ImGuiCol_ScrollbarGrabHovered] = surfaceActive;
            colors[ImGuiCol_ScrollbarGrabActive] = accentActive;
            colors[ImGuiCol_CheckMark] = accent;
            colors[ImGuiCol_SliderGrab] = accent;
            colors[ImGuiCol_SliderGrabActive] = accentHovered;
            colors[ImGuiCol_Button] = surface;
            colors[ImGuiCol_ButtonHovered] = surfaceHovered;
            colors[ImGuiCol_ButtonActive] = surfaceActive;
            colors[ImGuiCol_Header] = WithAlpha(accent, 0.25f);
            colors[ImGuiCol_HeaderHovered] = WithAlpha(accent, 0.35f);
            colors[ImGuiCol_HeaderActive] = WithAlpha(accent, 0.50f);
            colors[ImGuiCol_Separator] = border;
            colors[ImGuiCol_SeparatorHovered] = accentHovered;
            colors[ImGuiCol_SeparatorActive] = accent;
            colors[ImGuiCol_ResizeGrip] = WithAlpha(accent, 0.20f);
            colors[ImGuiCol_ResizeGripHovered] = WithAlpha(accent, 0.60f);
            colors[ImGuiCol_ResizeGripActive] = accent;
            colors[ImGuiCol_Tab] = backgroundDark;
            colors[ImGuiCol_TabHovered] = surfaceHovered;
            colors[ImGuiCol_TabSelected] = background;
            colors[ImGuiCol_TabSelectedOverline] = accent;
            colors[ImGuiCol_TabDimmed] = backgroundDark;
            colors[ImGuiCol_TabDimmedSelected] = background;
            colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(accent, 0.0f);
            colors[ImGuiCol_DockingPreview] = WithAlpha(accent, 0.40f);
            colors[ImGuiCol_DockingEmptyBg] = backgroundDark;
            colors[ImGuiCol_TextSelectedBg] = WithAlpha(accent, 0.35f);
            colors[ImGuiCol_NavCursor] = accent;

            style.FontSizeBase = fontSize;
            style.ScaleAllSizes(displayScale);
            style.FontScaleDpi = displayScale;
        }

        ImFont* LoadFonts() {
#ifdef _WIN32
            std::string windowsDir{ "C:\\Windows" };
            char* value{ nullptr };
            std::size_t length{};
            // _dupenv_s is MSVC's safe getenv.
            if (_dupenv_s(&value, &length, "WINDIR") == 0 && value) windowsDir = value;
            std::free(value);
            const std::string fonts{ windowsDir + "\\Fonts\\" };
            ImFont* const regular{ LoadFirstFont({ fonts + "segoeui.ttf" }) };
            ImFont* const bold{ LoadFirstFont({ fonts + "seguisb.ttf", fonts + "segoeuib.ttf" }) };
#else
            const std::string fonts{ "/usr/share/fonts/truetype/" };
            ImFont* const regular{ LoadFirstFont({ fonts + "ubuntu/Ubuntu-R.ttf",
                                                   fonts + "dejavu/DejaVuSans.ttf",
                                                   fonts + "liberation/LiberationSans-Regular.ttf",
                                                   fonts + "noto/NotoSans-Regular.ttf" }) };
            ImFont* const bold{ LoadFirstFont({ fonts + "ubuntu/Ubuntu-M.ttf",
                                                fonts + "dejavu/DejaVuSans-Bold.ttf",
                                                fonts + "liberation/LiberationSans-Bold.ttf",
                                                fonts + "noto/NotoSans-Bold.ttf" }) };
#endif
            ImGuiIO& io{ ImGui::GetIO() };
            io.FontDefault = regular ? regular : io.Fonts->AddFontDefaultVector();
            return bold ? bold : io.FontDefault;
        }

    }  // namespace theme
}  // namespace seamcarve
