// Interactive front end (Dear ImGui + GLFW + OpenGL), compiled into the main executable.
// This is only the window/UI scaffolding: load an image (type its path or drag and drop it onto the
// window), pick an algorithm, choose a target size, press "Carve" and save the result. The seam
// carving itself lives in the core library; until it is implemented the status bar shows its error.
#include "seamcarve/gui.hpp"
#include "seamcarve/gui_theme.hpp"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>  // DockBuilder (default window layout), BeginViewportSideBar (status bar), IM_PI
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "seamcarve/seam_carver.hpp"
#include "seamcarve/types.hpp"

namespace seamcarve {
    namespace {
        /// Everything the UI needs to remember between frames.
        struct GuiState {
            cv::Mat original;
            cv::Mat result;
            GLuint originalTexture{};
            GLuint resultTexture{};
            char pathBuffer[512]{};
            int algorithmIndex{};  // 0 = DP, 1 = greedy, 2 = graph cut (same order as Algorithm)
            int targetWidth{};
            int targetHeight{};
            int scalePercent{ 100 };      // target size as a percentage of the original (Scale % slider)
            bool keepAspectRatio{ true }; // width and height sliders move together
            bool fitToWindow{ true };     // scale the images to fit their panels (otherwise 100% with scrollbars)
            float viewScale{ 1.0f };      // display scale shared by the original and the result so both match
            ImVec2 originalPanelSize{};   // free space in the Original / Result panels, measured each frame
            ImVec2 resultPanelSize{};     // (used for the next frame's fit scale)
            std::string loadedPath;       // path of the image that is currently loaded (used to name saved results)
            std::string status;           // message in the status bar
            std::string statusDetail;     // extra text shown when hovering the message (e.g. a full path)
            bool statusIsError{ false };
            bool layoutChecked{ false };  // the default dock layout is created at most once
            std::string droppedPath;      // file dropped onto the window, loaded at the start of the next frame
            int droppedCount{};           // how many files were in that drop (only the first is used)
            ImFont* boldFont{};           // bold UI font (headings); the regular one is ImGui's default font
            bool underWsl{ false };       // running under WSL: files dropped from Windows apps never arrive, and
                                          // typed Windows paths (C:\...) are converted to /mnt/c/...
        };

        /**
        * @brief Checks whether the program runs under WSL (Windows Subsystem for Linux), where
        * dragging files from Windows apps onto a Linux window does not work.
        * @return True under WSL, false on Windows and on a normal Linux system.
        */
        bool IsRunningUnderWsl() {
#ifdef _WIN32
            return false;
#else
            std::ifstream versionFile{ "/proc/version" };  // e.g. "Linux version 6.6.87.2-microsoft-standard-WSL2 ..."
            std::string version;
            std::getline(versionFile, version);
            std::transform(version.begin(), version.end(), version.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return version.find("microsoft") != std::string::npos;
#endif
        }

        /**
        * @brief Prints GLFW errors to stderr.
        * @param[in] error	        - GLFW error code.
        * @param[in] description    - Human-readable description.
        */
        void GlfwErrorCallback(int error, const char* description) {
            std::cerr << "GLFW error " << error << ": " << description << "\n";
        }

        /**
        * @brief Sets the status bar message.
        * @param[in,out] state	- UI state.
        * @param[in] message	- Message to show.
        * @param[in] isError	- Shows the message in red when true, green otherwise.
        * @param[in] detail	- Optional extra text shown when hovering the message.
        */
        void SetStatus(GuiState& state, std::string message, bool isError, std::string detail = {}) {
            state.status = std::move(message);
            state.statusDetail = std::move(detail);
            state.statusIsError = isError;
        }

        /**
        * @brief Returns the file name part of a path ("C:/a/b.png" -> "b.png").
        * @param[in] path	- File path.
        * @return The file name.
        */
        std::string FileName(const std::string& path) {
            return std::filesystem::path{ path }.filename().string();
        }

        /**
        * @brief Uploads a BGR image into an OpenGL texture, creating the texture on first use.
        * @param[in] image	- 8-bit BGR image.
        * @param[in,out] textureId	- Texture handle; 0 creates a new texture.
        */
        void UploadTexture(const cv::Mat& image, GLuint& textureId) {
            cv::Mat rgb;
            cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);

            if (textureId == 0) glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
        }

        /**
        * @brief Cleans up a typed or pasted image path: trims spaces and removes surrounding quotes
        * (File Explorer's "Copy as path" adds them). On Linux, when the path as typed does not exist,
        * it is treated as a Windows path: backslashes become '/', and under WSL "C:\..." becomes "/mnt/c/...".
        * @param[in] path	- Path as typed.
        * @param[in] underWsl	- Whether the program runs under WSL.
        * @return The path to open.
        */
        std::string ResolveImagePath(std::string path, bool underWsl) {
            const auto notSpace{ [](unsigned char c) { return !std::isspace(c); } };
            path.erase(path.begin(), std::find_if(path.begin(), path.end(), notSpace));
            path.erase(std::find_if(path.rbegin(), path.rend(), notSpace).base(), path.end());
            if (path.size() >= 2 && path.front() == '"' && path.back() == '"') 
                path = path.substr(1, path.size() - 2);

#ifdef _WIN32
            (void)underWsl;
#else
            std::error_code error;
            if (std::filesystem::exists(path, error)) return path;  // a real Linux path, keep it as it is

            std::replace(path.begin(), path.end(), '\\', '/');
            const bool hasDriveLetter{ path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && path[2] == '/' };
            if (underWsl && hasDriveLetter) {
                path = "/mnt/" + std::string(1, static_cast<char>(std::tolower(static_cast<unsigned char>(path[0])))) + path.substr(2);
            }
#endif
            return path;
        }

        /**
        * @brief Loads the image whose path is in the path box (see ResolveImagePath()) and resets the
        * size sliders to its dimensions. The path box is updated to the path that was actually opened.
        * @param[in,out] state	- UI state to update (status bar is set on failure).
        */
        void LoadImage(GuiState& state) {
            const std::string path{ ResolveImagePath(state.pathBuffer, state.underWsl) };
            if (path.size() < sizeof(state.pathBuffer)) {
                path.copy(state.pathBuffer, sizeof(state.pathBuffer) - 1);
                state.pathBuffer[path.size()] = '\0';
            }

            const cv::Mat image{ cv::imread(path, cv::IMREAD_COLOR) };
            if (image.empty()) {
                SetStatus(state, "Cannot read image '" + FileName(path) + "'", true, path);
                return;
            }

            state.original = image;
            state.result.release();
            state.loadedPath = path;
            state.targetWidth = image.cols;
            state.targetHeight = image.rows;
            state.scalePercent = 100;
            UploadTexture(state.original, state.originalTexture);
            SetStatus(state, "Loaded " + FileName(state.loadedPath), false, state.loadedPath);
        }

        /**
        * @brief GLFW callback for files dragged and dropped onto the window. It only remembers the
        * first path; the image is loaded later by LoadDroppedImage() in the main loop, so that all
        * loading goes through the same code as the "Load" button.
        * @param[in] window	- Window the files were dropped on (its user pointer is the GuiState).
        * @param[in] count	- Number of dropped files.
        * @param[in] paths	- UTF-8 paths of the dropped files.
        */
        void DropCallback(GLFWwindow* window, int count, const char** paths) {
            GuiState* const state{ static_cast<GuiState*>(glfwGetWindowUserPointer(window)) };
            if (!state || count < 1 || !paths || !paths[0]) return;

            state->droppedPath = paths[0];
            state->droppedCount = count;
        }

        /**
        * @brief Loads the file remembered by DropCallback() (if any) and puts its path in the path box.
        * @param[in,out] state	- UI state; the pending drop is cleared.
        */
        void LoadDroppedImage(GuiState& state) {
            if (state.droppedPath.empty()) return;

            const std::string path{ std::move(state.droppedPath) };
            const int count{ state.droppedCount };
            state.droppedPath.clear();
            state.droppedCount = 0;

            if (path.size() >= sizeof(state.pathBuffer)) {
                SetStatus(state, "Dropped file path is too long (max " + std::to_string(sizeof(state.pathBuffer) - 1) + " characters)", true, path);
                return;
            }

            path.copy(state.pathBuffer, sizeof(state.pathBuffer) - 1);
            state.pathBuffer[path.size()] = '\0';
            LoadImage(state);

            if (count > 1 && !state.statusIsError) state.status += " (" + std::to_string(count) + " files dropped, used the first)";
        }

        /**
        * @brief Runs seam carving with the selected algorithm and target size.
        * @param[in,out] state	- UI state; the result texture and status bar are updated.
        */
        void RunCarve(GuiState& state) {
            CarveOptions options{};
            options.algorithm = static_cast<Algorithm>(state.algorithmIndex);

            try {
                state.result = Carve(state.original, state.targetWidth, state.targetHeight, options);
                UploadTexture(state.result, state.resultTexture);
                SetStatus(state, "Carved to " + std::to_string(state.result.cols) + " \xC3\x97 " + std::to_string(state.result.rows), false);  // UTF-8 multiplication sign
            } catch (const std::exception& e) {
                SetStatus(state, std::string("Error: ") + e.what(), true);
            }
        }

        /**
        * @brief Scales an image dimension by a factor, rounded and kept within [1, size].
        * @param[in] size	- Original dimension in pixels.
        * @param[in] factor	- Scale factor (1.0 keeps the size).
        * @return The scaled dimension.
        */
        int ScaleDimension(int size, double factor) {
            const int scaled{ static_cast<int>(std::lround(size * factor)) };
            return std::clamp(scaled, 1, size);
        }

        /**
        * @brief Builds the file name for a saved result: "<name>_carved_<w>x<h><ext>" next to the
        * loaded image (".png" if the loaded file has no extension).
        * @param[in] sourcePath	- Path of the loaded image.
        * @param[in] width	- Width of the result.
        * @param[in] height	- Height of the result.
        * @return Path to save the result to.
        */
        std::string MakeResultPath(const std::string& sourcePath, int width, int height) {
            std::filesystem::path path{ sourcePath };
            std::string extension{ path.extension().string() };
            if (extension.empty()) extension = ".png";

            path.replace_filename(path.stem().string() + "_carved_" + std::to_string(width) + "x" + std::to_string(height) + extension);
            return path.string();
        }

        /**
        * @brief Saves the carved result next to the loaded image (see MakeResultPath()).
        * @param[in,out] state	- UI state; the status bar reports where it was saved or the error.
        */
        void SaveResult(GuiState& state) {
            const std::string path{ MakeResultPath(state.loadedPath, state.result.cols, state.result.rows) };

            bool saved{ false };
            try {
                saved = cv::imwrite(path, state.result);
            } catch (const std::exception&) {
                saved = false;  // OpenCV throws for unsupported formats; report it like any other failure
            }

            if (saved) SetStatus(state, "Saved " + FileName(path), false, path);
            else SetStatus(state, "Cannot save " + FileName(path), true, path);
        }

        // ---- Custom widgets and drawing ---------------------------------------------------------

        /**
        * @brief Draws a full-width button in the accent colour (the main action of a panel). When
        * disabled it is drawn grey instead, so it clearly cannot be clicked.
        * @param[in] label	- Button label.
        * @param[in] enabled	- Whether the button can be clicked.
        * @return True when clicked.
        */
        bool PrimaryButton(const char* label, bool enabled) {
            if (enabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::accentHovered);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::accentActive);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::textOnAccent);
            }
            ImGui::BeginDisabled(!enabled);
            const bool clicked{ ImGui::Button(label, ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() * theme::primaryButtonHeight }) };
            ImGui::EndDisabled();
            if (enabled) ImGui::PopStyleColor(4);
            return clicked;
        }

        /**
        * @brief Draws text centred on a point with the current font.
        * @param[in,out] drawList	- Draw list to draw into.
        * @param[in] center	- Centre of the text.
        * @param[in] color	- Text colour.
        * @param[in] text	- Text to draw.
        */
        void DrawCenteredText(ImDrawList* drawList, const ImVec2& center, ImU32 color, const char* text) {
            const ImVec2 size{ ImGui::CalcTextSize(text) };
            drawList->AddText(ImVec2{ std::floor(center.x - size.x * 0.5f), std::floor(center.y - size.y * 0.5f) }, color, text);
        }

        /**
        * @brief Draws a rounded rectangle with a dashed outline (straight edges dashed, corners solid).
        * @param[in,out] drawList	- Draw list to draw into.
        * @param[in] min	- Top-left corner.
        * @param[in] max	- Bottom-right corner.
        * @param[in] color	- Line colour.
        * @param[in] rounding	- Corner radius.
        * @param[in] thickness	- Line thickness.
        * @param[in] dash	- Length of each dash.
        * @param[in] gap	- Gap between dashes.
        */
        void DrawDashedRect(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, ImU32 color, float rounding,
                            float thickness, float dash, float gap) {
            const float r{ std::min(rounding, std::min(max.x - min.x, max.y - min.y) * 0.5f) };

            // Corners: quarter circles.
            drawList->PathArcTo(ImVec2{ min.x + r, min.y + r }, r, IM_PI, IM_PI * 1.5f);
            drawList->PathStroke(color, 0, thickness);
            drawList->PathArcTo(ImVec2{ max.x - r, min.y + r }, r, IM_PI * 1.5f, IM_PI * 2.0f);
            drawList->PathStroke(color, 0, thickness);
            drawList->PathArcTo(ImVec2{ max.x - r, max.y - r }, r, 0.0f, IM_PI * 0.5f);
            drawList->PathStroke(color, 0, thickness);
            drawList->PathArcTo(ImVec2{ min.x + r, max.y - r }, r, IM_PI * 0.5f, IM_PI);
            drawList->PathStroke(color, 0, thickness);

            // Edges: dashes from one corner to the next.
            const auto dashedLine{ [&](const ImVec2& from, const ImVec2& to) {
                const ImVec2 delta{ to.x - from.x, to.y - from.y };
                const float length{ std::sqrt(delta.x * delta.x + delta.y * delta.y) };
                if (length <= 0.0f) return;
                const ImVec2 direction{ delta.x / length, delta.y / length };
                for (float start{ gap * 0.5f }; start < length; start += dash + gap) {
                    const float end{ std::min(start + dash, length) };
                    drawList->AddLine(ImVec2{ from.x + direction.x * start, from.y + direction.y * start },
                                      ImVec2{ from.x + direction.x * end, from.y + direction.y * end }, color, thickness);
                }
            } };
            dashedLine(ImVec2{ min.x + r, min.y }, ImVec2{ max.x - r, min.y });
            dashedLine(ImVec2{ max.x, min.y + r }, ImVec2{ max.x, max.y - r });
            dashedLine(ImVec2{ max.x - r, max.y }, ImVec2{ min.x + r, max.y });
            dashedLine(ImVec2{ min.x, max.y - r }, ImVec2{ min.x, min.y + r });
        }

        /**
        * @brief Draws a simple "picture" icon (frame, sun and two mountains) centred on a point.
        * @param[in,out] drawList	- Draw list to draw into.
        * @param[in] center	- Centre of the icon.
        * @param[in] unit	- Size unit (the current font size), so the icon scales with the UI.
        * @param[in] color	- Icon colour.
        */
        void DrawPictureIcon(ImDrawList* drawList, const ImVec2& center, float unit, ImU32 color) {
            const float width{ unit * 3.4f };
            const float height{ unit * 2.6f };
            const ImVec2 min{ center.x - width * 0.5f, center.y - height * 0.5f };
            const ImVec2 max{ center.x + width * 0.5f, center.y + height * 0.5f };
            const float baseline{ max.y - height * 0.16f };

            drawList->AddRect(min, max, color, unit * 0.35f, 0, unit * 0.14f);
            drawList->AddCircleFilled(ImVec2{ min.x + width * 0.72f, min.y + height * 0.32f }, height * 0.11f, color);
            drawList->AddTriangleFilled(ImVec2{ min.x + width * 0.14f, baseline }, 
                                        ImVec2{ min.x + width * 0.40f, min.y + height * 0.36f },
                                        ImVec2{ min.x + width * 0.66f, baseline }, 
                                        color);
            drawList->AddTriangleFilled(ImVec2{ min.x + width * 0.50f, baseline }, 
                                        ImVec2{ min.x + width * 0.68f, min.y + height * 0.54f },
                                        ImVec2{ min.x + width * 0.86f, baseline }, 
                                        color);
        }

        /**
        * @brief Fills the current window with the "no image yet" drop zone: a dashed box with an icon
        * and instructions to drag and drop an image or type its path (only the latter under WSL,
        * where drag and drop from Windows does not work).
        * @param[in] state	- UI state (bold font, whether files can be dropped).
        */
        void DrawDropZone(const GuiState& state) {
            ImDrawList* const drawList{ ImGui::GetWindowDrawList() };
            const float unit{ ImGui::GetFontSize() };
            const ImVec2 origin{ ImGui::GetCursorScreenPos() };
            const ImVec2 available{ ImGui::GetContentRegionAvail() };
            if (available.x < unit * 4.0f || available.y < unit * 4.0f) return;  // panel too small to draw anything useful

            const ImVec2 min{ origin.x + unit * 0.5f, origin.y + unit * 0.5f };
            const ImVec2 max{ origin.x + available.x - unit * 0.5f, origin.y + available.y - unit * 0.5f };
            const ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };

            drawList->AddRectFilled(min, max, ImGui::GetColorU32(theme::dropZoneFill), unit);
            DrawDashedRect(drawList, min, max, ImGui::GetColorU32(theme::dropZoneBorder), unit, unit * 0.12f, unit * 0.7f, unit * 0.45f);
            DrawPictureIcon(drawList, ImVec2{ center.x, center.y - unit * 2.6f }, unit, ImGui::GetColorU32(theme::dropZoneIcon));

            ImGui::PushFont(state.boldFont, ImGui::GetStyle().FontSizeBase * theme::headingFontScale);
            DrawCenteredText(drawList, ImVec2{ center.x, center.y + unit * 0.4f }, ImGui::GetColorU32(theme::text),
                             !state.underWsl ? "Drop an image here" : "No image loaded");
            ImGui::PopFont();

            DrawCenteredText(drawList, ImVec2{ center.x, center.y + unit * 2.0f }, ImGui::GetColorU32(theme::textMuted),
                             !state.underWsl ? "or type its path in Controls and press Load" : "Type its path in Controls and press Load");
            DrawCenteredText(drawList, ImVec2{ center.x, center.y + unit * 3.3f }, ImGui::GetColorU32(theme::textFaint),
                             "PNG  \xC2\xB7  JPG  \xC2\xB7  BMP");
        }

        /**
        * @brief Draws a muted hint centred in the current window (used when a panel has nothing to show).
        * @param[in] text	- Hint text.
        */
        void DrawPanelHint(const char* text) {
            const ImVec2 origin{ ImGui::GetCursorScreenPos() };
            const ImVec2 available{ ImGui::GetContentRegionAvail() };
            DrawCenteredText(ImGui::GetWindowDrawList(), ImVec2{ origin.x + available.x * 0.5f, origin.y + available.y * 0.5f },
                             ImGui::GetColorU32(theme::textMuted), text);
        }

        // ---- Panels -------------------------------------------------------------------------------

        /**
        * @brief Draws the target size widgets: keep-aspect-ratio checkbox, Reset button, Scale % /
        * Width / Height sliders and how many seams the current target removes.
        * @param[in,out] state	- UI state edited by the widgets.
        */
        void DrawSizeControls(GuiState& state) {
            const int originalWidth{ state.original.cols };
            const int originalHeight{ state.original.rows };
            const ImGuiSliderFlags sliderFlags{ ImGuiSliderFlags_AlwaysClamp };  // typed values (Ctrl+click) stay in range

            ImGui::SeparatorText("Target size");

            if (ImGui::Checkbox("Keep aspect ratio", &state.keepAspectRatio) && state.keepAspectRatio) {
                state.targetHeight = ScaleDimension(originalHeight, static_cast<double>(state.targetWidth) / originalWidth);
            }

            // Reset, right-aligned on the same row as the checkbox.
            const float resetWidth{ ImGui::CalcTextSize("Reset").x + ImGui::GetStyle().FramePadding.x * 2.0f };
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - resetWidth));
            if (ImGui::Button("Reset")) {
                state.targetWidth = originalWidth;
                state.targetHeight = originalHeight;
                state.scalePercent = 100;
            }
            ImGui::SetItemTooltip("Back to the original size");

            if (ImGui::SliderInt("Scale", &state.scalePercent, 1, 100, "%d%%", sliderFlags)) {
                state.targetWidth = ScaleDimension(originalWidth, state.scalePercent / 100.0);
                state.targetHeight = ScaleDimension(originalHeight, state.scalePercent / 100.0);
            }

            if (ImGui::SliderInt("Width", &state.targetWidth, 1, originalWidth, "%d px", sliderFlags)) {
                if (state.keepAspectRatio) state.targetHeight = ScaleDimension(originalHeight, static_cast<double>(state.targetWidth) / originalWidth);
                state.scalePercent = ScaleDimension(100, static_cast<double>(state.targetWidth) / originalWidth);
            }

            if (ImGui::SliderInt("Height", &state.targetHeight, 1, originalHeight, "%d px", sliderFlags)) {
                if (state.keepAspectRatio) state.targetWidth = ScaleDimension(originalWidth, static_cast<double>(state.targetHeight) / originalHeight);
                state.scalePercent = ScaleDimension(100, static_cast<double>(state.targetHeight) / originalHeight);
            }

            const int verticalSeams{ originalWidth - state.targetWidth };
            const int horizontalSeams{ originalHeight - state.targetHeight };
            // \xC3\x97 is the UTF-8 multiplication sign; it is in every UI font (unlike arrows, which the Ubuntu font lacks).
            ImGui::TextDisabled("From %d \xC3\x97 %d to %d \xC3\x97 %d px", originalWidth, originalHeight, state.targetWidth, state.targetHeight);
            ImGui::TextDisabled("Removes %d vertical + %d horizontal seams", verticalSeams, horizontalSeams);
        }

        /**
        * @brief Draws the control panel: image path + Load, algorithm, target size, Carve / Save
        * buttons and view options.
        * @param[in,out] state	- UI state edited by the widgets.
        */
        void DrawControls(GuiState& state) {
            ImGui::Begin("Controls");

            ImGui::SeparatorText("Image");
            const float loadWidth{ ImGui::CalcTextSize("Load").x + ImGui::GetStyle().FramePadding.x * 2.0f };
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - loadWidth - ImGui::GetStyle().ItemSpacing.x);
            const bool enterPressed{ ImGui::InputTextWithHint("##path", "Path to an image...", state.pathBuffer, sizeof(state.pathBuffer),
                                                              ImGuiInputTextFlags_EnterReturnsTrue) };
            ImGui::SameLine();
            if (ImGui::Button("Load") || enterPressed) LoadImage(state);
            if (!state.underWsl) ImGui::TextDisabled("or drag and drop an image onto the window");

            if (!state.original.empty()) {
                ImGui::SeparatorText("Algorithm");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::Combo("##algorithm", &state.algorithmIndex, "Dynamic programming\0Greedy\0Graph cut\0");

                DrawSizeControls(state);

                ImGui::Dummy(ImVec2{ 0.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f });

                // Nothing to carve when the target is the original size.
                const bool sameSize{ state.targetWidth == state.original.cols && state.targetHeight == state.original.rows };
                if (PrimaryButton("Carve", !sameSize)) RunCarve(state);
                if (sameSize) ImGui::SetItemTooltip("Make the width or height smaller first");

                if (!state.result.empty()) {
                    if (ImGui::Button("Save result", ImVec2{ -FLT_MIN, 0.0f })) SaveResult(state);
                    ImGui::SetItemTooltip("Saves %s", MakeResultPath(state.loadedPath, state.result.cols, state.result.rows).c_str());
                }

                ImGui::SeparatorText("View");
                ImGui::Checkbox("Fit images to window", &state.fitToWindow);
            }

            ImGui::End();
        }

        /**
        * @brief Draws an image (inside the current window) at the given display scale.
        * @param[in] image	- Image being shown (for its size).
        * @param[in] texture	- OpenGL texture holding the image.
        * @param[in] scale	- Display scale (1.0 = 100%).
        */
        void DrawScaledImage(const cv::Mat& image, GLuint texture, float scale) {
            ImGui::Image((ImTextureID)(std::intptr_t)texture,
                         ImVec2{ std::floor(image.cols * scale), std::floor(image.rows * scale) });
        }

        /**
        * @brief Scale at which an image fits completely inside a panel.
        * @param[in] image	- Image to fit.
        * @param[in] panelSize	- Free space in the panel (0 if not measured yet).
        * @return The fit scale, or a very large value if the panel size is not known (so it does not limit the scale).
        */
        float FitScale(const cv::Mat& image, const ImVec2& panelSize) {
            if (panelSize.x <= 1.0f || panelSize.y <= 1.0f) return 1.0e6f;
            return std::min(panelSize.x / image.cols, panelSize.y / image.rows);
        }

        /**
        * @brief Draws the Original and Result windows. With no image loaded, Original shows the drop
        * zone; with no result yet, Result shows a hint. Both images use the same scale so they can be
        * compared (the result shows up smaller, as it really is). In fit mode the scale is the largest
        * one at which both images fit in their panels; otherwise both are shown at 100% and the
        * windows scroll. Panel sizes are measured while drawing and used the next frame.
        * @param[in,out] state	- UI state holding the images and textures (viewScale and panel sizes are updated).
        */
        void DrawImages(GuiState& state) {
            state.viewScale = 1.0f;
            if (state.fitToWindow && !state.original.empty()) {
                float scale{ FitScale(state.original, state.originalPanelSize) };
                if (!state.result.empty()) scale = std::min(scale, FitScale(state.result, state.resultPanelSize));
                if (scale < 1.0e6f) state.viewScale = scale;
            }

            const ImGuiWindowFlags flags{ state.fitToWindow ? ImGuiWindowFlags_NoScrollbar : ImGuiWindowFlags_HorizontalScrollbar };

            if (ImGui::Begin("Original", nullptr, flags)) {
                state.originalPanelSize = ImGui::GetContentRegionAvail();
                if (state.original.empty()) DrawDropZone(state);
                else DrawScaledImage(state.original, state.originalTexture, state.viewScale);
            }
            ImGui::End();

            if (ImGui::Begin("Result", nullptr, flags)) {
                state.resultPanelSize = ImGui::GetContentRegionAvail();
                if (!state.result.empty()) DrawScaledImage(state.result, state.resultTexture, state.viewScale);
                else if (state.original.empty()) DrawPanelHint("The carved image will appear here");
                else DrawPanelHint("Set a target size and press Carve");
            }
            ImGui::End();
        }

        /**
        * @brief Draws the status bar along the bottom of the window: the last message on the left
        * (hover it for details such as the full path) and the image / zoom info on the right. Must
        * be called before DrawDockspace() so the dock space leaves room for it.
        * @param[in] state	- UI state to show.
        */
        void DrawStatusBar(const GuiState& state) {
            const ImGuiWindowFlags flags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar };
            if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(), flags)) {
                if (ImGui::BeginMenuBar()) {
                    // Left: coloured dot + message.
                    const bool hasStatus{ !state.status.empty() };
                    const ImVec4 color{ !hasStatus ? theme::textMuted : (state.statusIsError ? theme::error : theme::success) };
                    const float dotRadius{ ImGui::GetFontSize() * 0.22f };
                    const ImVec2 cursor{ ImGui::GetCursorScreenPos() };
                    const float textCenterY{ cursor.y + ImGui::GetStyle().FramePadding.y + ImGui::GetTextLineHeight() * 0.5f };  // menu bar text is offset by the frame padding
                    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2{ cursor.x + dotRadius, textCenterY },
                                                                dotRadius, ImGui::GetColorU32(color));
                    ImGui::Dummy(ImVec2{ dotRadius * 2.0f, 0.0f });

                    ImGui::TextColored(hasStatus ? color : theme::textMuted, "%s", hasStatus ? state.status.c_str() : "Ready");
                    if (!state.statusDetail.empty()) ImGui::SetItemTooltip("%s", state.statusDetail.c_str());

                    // Right: what is loaded, the target size and the zoom.
                    std::string info{ "No image loaded" };
                    if (!state.original.empty()) {
                        info = FileName(state.loadedPath) + "   " + std::to_string(state.original.cols) + " \xC3\x97 " + std::to_string(state.original.rows) +
                               "   Zoom " + std::to_string(static_cast<int>(std::lround(state.viewScale * 100.0f))) + "%";
                    }
                    const float infoWidth{ ImGui::CalcTextSize(info.c_str()).x };
                    const float rightMargin{ ImGui::GetStyle().WindowPadding.x };
                    ImGui::SameLine();
                    const float freeWidth{ ImGui::GetContentRegionAvail().x - rightMargin };
                    if (freeWidth > infoWidth + ImGui::GetStyle().ItemSpacing.x * 2.0f) {  // hide it rather than overlap a long message
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + freeWidth - infoWidth);
                        ImGui::TextDisabled("%s", info.c_str());
                    }

                    ImGui::EndMenuBar();
                }
            }
            ImGui::End();
        }

        /**
        * @brief Creates the dock space over the window (above the status bar); on the first run (no
        * saved layout) docks the windows as Controls | Original | Result. Later runs restore the
        * layout saved in imgui.ini.
        * @param[in,out] state	- UI state (remembers that the layout was already checked).
        */
        void DrawDockspace(GuiState& state) {
            const ImGuiID dockspaceId{ ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport()) };
            if (state.layoutChecked) return;
            state.layoutChecked = true;

            const ImGuiDockNode* const node{ ImGui::DockBuilderGetNode(dockspaceId) };
            if (node && !node->IsEmpty()) return;  // a layout was loaded from imgui.ini

            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

            ImGuiID rightId{};
            const ImGuiID controlsId{ ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.27f, nullptr, &rightId) };
            ImGuiID resultId{};
            const ImGuiID originalId{ ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Left, 0.5f, nullptr, &resultId) };

            ImGui::DockBuilderDockWindow("Controls", controlsId);
            ImGui::DockBuilderDockWindow("Original", originalId);
            ImGui::DockBuilderDockWindow("Result", resultId);
            ImGui::DockBuilderFinish(dockspaceId);
        }
    }  // namespace

    int RunGui(const std::string& imagePath) {
        glfwSetErrorCallback(GlfwErrorCallback);
        if (!glfwInit()) return 1;

        const char* const glslVersion{ "#version 130" };
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

        // Size the window for the monitor's display scale (e.g. 150% on many laptops), but keep it
        // inside the screen.
        float displayScale{ 1.0f };
        int windowWidth{ 1280 }, windowHeight{ 800 };
        if (GLFWmonitor* const monitor{ glfwGetPrimaryMonitor() }) {
            float scaleX{ 1.0f }, scaleY{ 1.0f };
            glfwGetMonitorContentScale(monitor, &scaleX, &scaleY);
            if (scaleX > 0.0f) displayScale = scaleX;

            int workX{}, workY{}, workWidth{}, workHeight{};
            glfwGetMonitorWorkarea(monitor, &workX, &workY, &workWidth, &workHeight);
            windowWidth = static_cast<int>(windowWidth * displayScale);
            windowHeight = static_cast<int>(windowHeight * displayScale);
            if (workWidth > 0 && workHeight > 0) {
                windowWidth = std::min(windowWidth, workWidth * 9 / 10);
                windowHeight = std::min(windowHeight, workHeight * 9 / 10);
            }
        }

        GLFWwindow* const window{ glfwCreateWindow(windowWidth, windowHeight, "Seam Carving", nullptr, nullptr) };
        if (!window) {
            glfwTerminate();
            return 1;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io{ ImGui::GetIO() };
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigDpiScaleFonts = true;  // re-scale the fonts if the window moves to a monitor with another display scale
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glslVersion);

        GuiState state;
        theme::Apply(displayScale);
        state.boldFont = theme::LoadFonts();
        state.underWsl = IsRunningUnderWsl();

        if (!imagePath.empty()) {
            imagePath.copy(state.pathBuffer, sizeof(state.pathBuffer) - 1);
            LoadImage(state);
        }

        // Drag and drop: the ImGui GLFW backend does not use the window user pointer or the drop
        // callback, so both are free for us.
        glfwSetWindowUserPointer(window, &state);
        glfwSetDropCallback(window, DropCallback);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            LoadDroppedImage(state);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            DrawStatusBar(state);  // before the dock space, which fills the space left above it
            DrawDockspace(state);
            DrawControls(state);
            DrawImages(state);

            ImGui::Render();
            int framebufferWidth{}, framebufferHeight{};
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glClearColor(theme::backgroundDark.x, theme::backgroundDark.y, theme::backgroundDark.z, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        if (state.originalTexture != 0) glDeleteTextures(1, &state.originalTexture);
        if (state.resultTexture != 0) glDeleteTextures(1, &state.resultTexture);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

}  // namespace seamcarve
