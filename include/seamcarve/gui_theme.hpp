#pragma once
// Look and feel of the GUI: every colour, the base font size, the ImGui style (spacing, rounding)
// and the UI fonts. To restyle the app, change the values here; gui.cpp only uses the names.
#include <imgui.h>

namespace seamcarve {
    namespace theme {
        /**
        * @brief Returns a colour with a different alpha.
        * @param[in] color	- Base colour.
        * @param[in] alpha	- New alpha (0..1).
        * @return The colour with that alpha.
        */
        constexpr ImVec4 WithAlpha(const ImVec4& color, float alpha) {
            return ImVec4{ color.x, color.y, color.z, alpha };
        }

        // ---- Palette -----------------------------------------------------------------------
        // Panels.
        inline constexpr ImVec4 background{ 0.106f, 0.114f, 0.133f, 1.0f };
        // Title bars, status bar, gaps between panels.
        inline constexpr ImVec4 backgroundDark{ 0.078f, 0.086f, 0.102f, 1.0f };
        // Inputs, sliders, buttons.
        inline constexpr ImVec4 surface{ 0.153f, 0.165f, 0.192f, 1.0f };
        inline constexpr ImVec4 surfaceHovered{ 0.196f, 0.212f, 0.247f, 1.0f };
        inline constexpr ImVec4 surfaceActive{ 0.231f, 0.251f, 0.294f, 1.0f };
        inline constexpr ImVec4 border{ 0.212f, 0.227f, 0.263f, 1.0f };
        inline constexpr ImVec4 text{ 0.910f, 0.918f, 0.933f, 1.0f };
        inline constexpr ImVec4 textMuted{ 0.545f, 0.565f, 0.612f, 1.0f };
        // Labels on accent-coloured buttons.
        inline constexpr ImVec4 textOnAccent{ 1.0f, 1.0f, 1.0f, 1.0f };
        // #5788F5
        inline constexpr ImVec4 accent{ 0.341f, 0.533f, 0.961f, 1.0f };
        inline constexpr ImVec4 accentHovered{ 0.431f, 0.608f, 0.980f, 1.0f };
        inline constexpr ImVec4 accentActive{ 0.271f, 0.451f, 0.859f, 1.0f };
        inline constexpr ImVec4 success{ 0.380f, 0.800f, 0.522f, 1.0f };
        inline constexpr ImVec4 error{ 0.941f, 0.400f, 0.400f, 1.0f };

        // ---- Custom-drawn elements ---------------------------------------------------------
        inline constexpr ImVec4 dropZoneFill{ WithAlpha(accent, 0.05f) };
        inline constexpr ImVec4 dropZoneBorder{ WithAlpha(accent, 0.55f) };
        inline constexpr ImVec4 dropZoneIcon{ WithAlpha(accent, 0.85f) };
        // Least important hints (e.g. the list of file formats).
        inline constexpr ImVec4 textFaint{ WithAlpha(textMuted, 0.7f) };

        // ---- Sizes -------------------------------------------------------------------------
        // Base UI font size in pixels at 100% display scale.
        inline constexpr float fontSize{ 16.0f };
        // Large headings, relative to fontSize.
        inline constexpr float headingFontScale{ 1.5f };
        // Height of the main action button, in frame heights.
        inline constexpr float primaryButtonHeight{ 1.35f };

        /**
        * @brief Applies the app's colours, rounding and spacing on top of ImGui's dark style,
        * then scales everything for the monitor's display scale (e.g. 1.5 on a 150% Windows
        * display). Call once after ImGui::CreateContext().
        * @param[in] displayScale	- Monitor content scale.
        */
        void Apply(float displayScale);

        /**
        * @brief Loads the UI fonts from the operating system (Segoe UI on Windows, Ubuntu /
        * DejaVu / Liberation / Noto on Linux), so no font file has to be shipped with the
        * program, and makes the regular one ImGui's default font. Falls back to ImGui's
        * built-in font when none is found. The fonts are scalable, so one load covers every size.
        * @return The bold font for headings (the default font if no bold font was found).
        */
        ImFont* LoadFonts();

    }  // namespace theme
}  // namespace seamcarve
