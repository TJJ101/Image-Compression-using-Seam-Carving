// Interactive front end (Dear ImGui + GLFW + OpenGL), compiled into the main executable.
// This is only the window/UI scaffolding: load an image (type its path or drag and drop it onto the
// window), pick an algorithm and energy method, choose a target size, press "Carve" and save the result. The seam
// carving itself lives in the core library; until it is implemented the status bar shows its error.
// Carving runs on a worker thread (see StartCarve() / CheckCarveThread()) so the window stays responsive.
#include "seamcarve/gui.hpp"
#include "seamcarve/gui_theme.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
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
            int energyIndex{ static_cast<int>(EnergyMethod::SOBEL) };  // 0 = gradient, 1 = Sobel, 2 = divide and conquer (same order as EnergyMethod)
            int divideAndConquerIndex{};  // 0 = recursion, 1 = iteration (same order as DivideAndConquer)
            int baseSize{ DEFAULT_BASE_SIZE };  // block size where divide and conquer stops splitting
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

            // Background carve (see StartCarve() / CheckCarveThread()). Only the main thread touches GuiState;
            // the worker has its own copy of the image and talks back only through the future and the atomics.
            std::future<cv::Mat> carveJob;          // valid() from StartCarve() until CheckCarveThread() collects the result
            std::atomic<int> seamsDone{ 0 };        // progress, incremented by the worker
            std::atomic<bool> cancelCarve{ false }; // set by the Cancel button (or on exit), read by the worker
            int seamsTotal{};                       // seams the running carve removes
            std::chrono::steady_clock::time_point carveStart;
        };

        /// True while a carve is running on the worker thread (or its result has not been collected yet).
        bool IsCarving(const GuiState& state) {
            return state.carveJob.valid();
        }

        /**
        * @brief Formats the time since a point as seconds with two decimals, e.g. "1.23 s".
        * @param[in] start	- Start time.
        * @return The formatted elapsed time.
        */
        std::string ElapsedSince(std::chrono::steady_clock::time_point start) {
            const std::chrono::duration<double> elapsed{ std::chrono::steady_clock::now() - start };
            char text[32];
            std::snprintf(text, sizeof(text), "%.2f s", elapsed.count());
            return text;
        }

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
        * @brief Removes the loaded image and its result and puts the window back the way it is at start-up
        * @param[in,out] state	- UI state to reset.
        */
        void RemoveImage(GuiState& state) {
            if (IsCarving(state)) return;

            const std::string removed{ FileName(state.loadedPath) };
            if (state.originalTexture != 0) glDeleteTextures(1, &state.originalTexture);
            if (state.resultTexture != 0) glDeleteTextures(1, &state.resultTexture);
            state.originalTexture = 0;
            state.resultTexture = 0;

            const GuiState defaults{};  // start-up values, so each default is only written once (in GuiState)
            state.original.release();
            state.result.release();
            state.pathBuffer[0] = '\0';
            state.loadedPath.clear();
            state.algorithmIndex = defaults.algorithmIndex;
            state.energyIndex = defaults.energyIndex;
            state.divideAndConquerIndex = defaults.divideAndConquerIndex;
            state.baseSize = defaults.baseSize;
            state.targetWidth = defaults.targetWidth;
            state.targetHeight = defaults.targetHeight;
            state.scalePercent = defaults.scalePercent;
            state.keepAspectRatio = defaults.keepAspectRatio;
            state.fitToWindow = defaults.fitToWindow;
            state.viewScale = defaults.viewScale;
            SetStatus(state, "Removed " + removed, false);
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

            if (IsCarving(state)) {  // the result of the running carve would no longer match the loaded image
                SetStatus(state, "Wait for the carve to finish (or cancel it) before loading another image", true, path);
                return;
            }

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
        * @brief Block size range allowed for the loaded image and divide-and-conquer type.
        * Recursion needs blocks of at least 3 px (its halves overlap by a pixel); iteration works
        * down to 1. At the image's larger side nothing is split, so the result equals plain Sobel.
        * (The maximum is kept at least the minimum so the range stays valid for images under 3 px.)
        * @param[in] state	- UI state (loaded image and divide-and-conquer type).
        * @return The {minimum, maximum} block size in pixels.
        */
        std::pair<int, int> BaseSizeRange(const GuiState& state) {
            const int minBaseSize{ static_cast<DivideAndConquer>(state.divideAndConquerIndex) == DivideAndConquer::RECURSION ? 3 : 1 };
            return { minBaseSize, std::max({ state.original.cols, state.original.rows, minBaseSize }) };
        }

        /**
        * @brief Block size actually used: the chosen one, limited to BaseSizeRange(). The chosen
        * value itself is never changed, so it comes back when a larger image or the other type
        * allows it again.
        * @param[in] state	- UI state.
        * @return The block size in pixels.
        */
        int EffectiveBaseSize(const GuiState& state) {
            const auto [minBaseSize, maxBaseSize]{ BaseSizeRange(state) };
            return std::clamp(state.baseSize, minBaseSize, maxBaseSize);
        }

        /**
        * @brief Runs seam carving with the selected algorithm, energy method and target size.
        * @param[in,out] state	- UI state; the result texture and status bar are updated.
        */
        void StartCarve(GuiState& state) {
            if (IsCarving(state)) return;

            CarveOptions options{};
            options.algorithm = static_cast<Algorithm>(state.algorithmIndex);
            options.seamsDone = &state.seamsDone;  // state outlives the job: RunGui() waits for it before returning
            options.cancel = &state.cancelCarve;

            state.seamsDone = 0;
            state.cancelCarve = false;
            state.seamsTotal = (state.original.cols - state.targetWidth) + (state.original.rows - state.targetHeight);
            state.carveStart = std::chrono::steady_clock::now();
            options.energyMethod = static_cast<EnergyMethod>(state.energyIndex);
            options.divideAndConquerType = static_cast<DivideAndConquer>(state.divideAndConquerIndex);
            options.baseSize = EffectiveBaseSize(state);

            try {
                // The worker gets its own copy of the image, so nothing the UI does can change it mid-carve.
                state.carveJob = std::async(std::launch::async,
                    [image = state.original.clone(), width = state.targetWidth, height = state.targetHeight, options] {
                        return Carve(image, width, height, options);
                    });
                SetStatus(state, "Carving...", false);
            } catch (const std::exception& e) {  // std::system_error when no thread can be started
                SetStatus(state, std::string("Error: cannot start carving: ") + e.what(), true);
            }
        }

        /**
        * @brief Called every frame: if the worker has finished, collects its result (or error) and shows
        * it. The texture upload happens here because OpenGL may only be used from the main thread.
        * @param[in,out] state	- UI state; the result texture and status bar are updated.
        */
        void CheckCarveThread(GuiState& state) {
            if (!IsCarving(state) || state.carveJob.wait_for(std::chrono::seconds{ 0 }) != std::future_status::ready) return;

            const std::string elapsed{ ElapsedSince(state.carveStart) };
            try {
                cv::Mat result{ state.carveJob.get() };  // rethrows anything Carve() threw; the future becomes invalid
                if (state.cancelCarve) {  // finished before it noticed the cancel; the user no longer wants it
                    SetStatus(state, "Carve cancelled", false);
                    return;
                }
                state.result = std::move(result);
                UploadTexture(state.result, state.resultTexture);
                SetStatus(state, "Carved to " + std::to_string(state.result.cols) + " \xC3\x97 " + std::to_string(state.result.rows) +  // UTF-8 multiplication sign
                                 " in " + elapsed, false);
            } catch (const CarveCancelled&) {
                SetStatus(state, "Carve cancelled after " + elapsed, false);
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
                state.scalePercent = ScaleDimension(100, static_cast<double>(state.targetWidth) / originalWidth);
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

            // With the aspect ratio unlocked, Width and Height are independent and leave Scale % alone.
            if (ImGui::SliderInt("Width", &state.targetWidth, 1, originalWidth, "%d px", sliderFlags) && state.keepAspectRatio) {
                state.targetHeight = ScaleDimension(originalHeight, static_cast<double>(state.targetWidth) / originalWidth);
                state.scalePercent = ScaleDimension(100, static_cast<double>(state.targetWidth) / originalWidth);
            }

            if (ImGui::SliderInt("Height", &state.targetHeight, 1, originalHeight, "%d px", sliderFlags) && state.keepAspectRatio) {
                state.targetWidth = ScaleDimension(originalWidth, static_cast<double>(state.targetHeight) / originalHeight);
                state.scalePercent = ScaleDimension(100, static_cast<double>(state.targetHeight) / originalHeight);
            }

            const int verticalSeams{ originalWidth - state.targetWidth };
            const int horizontalSeams{ originalHeight - state.targetHeight };
            // \xC3\x97 is the UTF-8 multiplication sign; it is in every UI font (unlike arrows, which the Ubuntu font lacks).
            ImGui::TextDisabled("From %d \xC3\x97 %d to %d \xC3\x97 %d px", originalWidth, originalHeight, state.targetWidth, state.targetHeight);
            ImGui::TextDisabled("Removes %d vertical + %d horizontal seams", verticalSeams, horizontalSeams);
        }

        /**
        * @brief Draws the progress of the running carve (seams removed so far, elapsed time) and a
        * Cancel button, in place of the Carve button.
        * @param[in,out] state	- UI state; Cancel sets cancelCarve.
        */
        void DrawCarveProgress(GuiState& state) {
            const int done{ state.seamsDone.load() };
            const float fraction{ state.seamsTotal > 0 ? std::min(1.0f, static_cast<float>(done) / state.seamsTotal) : 0.0f };
            const std::string label{ std::to_string(done) + " / " + std::to_string(state.seamsTotal) + " seams" };
            ImGui::ProgressBar(fraction, ImVec2{ -FLT_MIN, ImGui::GetFrameHeight() * theme::primaryButtonHeight }, label.c_str());

            // "###" keeps the same ID when the label changes.
            const bool cancelling{ state.cancelCarve.load() };
            ImGui::BeginDisabled(cancelling);
            if (ImGui::Button(cancelling ? "Cancelling...###cancelCarve" : "Cancel###cancelCarve", ImVec2{ -FLT_MIN, 0.0f })) state.cancelCarve = true;
            ImGui::EndDisabled();
            ImGui::TextDisabled("Elapsed %s", ElapsedSince(state.carveStart).c_str());
        }

        /**
        * @brief Draws the energy widgets: the energy method and, for divide and conquer only, its
        * type (recursion / iteration) and block size.
        * @param[in,out] state	- UI state edited by the widgets.
        */
        void DrawEnergyControls(GuiState& state) {
            ImGui::SeparatorText("Energy");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::Combo("##energy", &state.energyIndex, "Gradient\0Sobel\0Divide and conquer\0");
            ImGui::SetItemTooltip("How the energy map (which pixels matter) is computed");

            if (static_cast<EnergyMethod>(state.energyIndex) != EnergyMethod::DIVIDE_AND_CONQUER) return;

            ImGui::Combo("Type", &state.divideAndConquerIndex, "Recursion\0Iteration\0");

            // The slider edits a limited copy, so the chosen size is only changed when the user moves it
            // (not just because a smaller image or the recursion minimum limits it for now).
            const auto [minBaseSize, maxBaseSize]{ BaseSizeRange(state) };
            int shownBaseSize{ EffectiveBaseSize(state) };
            if (ImGui::SliderInt("Block size", &shownBaseSize, minBaseSize, maxBaseSize, "%d px", ImGuiSliderFlags_AlwaysClamp)) {
                state.baseSize = shownBaseSize;
            }
            ImGui::SetItemTooltip("Blocks this size or smaller are computed directly instead of being split");
        }

        /**
        * @brief Draws the control panel: image path + Load / Remove, algorithm, energy, target size,
        * Carve / Save buttons and view options.
        * @param[in,out] state	- UI state edited by the widgets.
        */
        void DrawControls(GuiState& state) {
            ImGui::Begin("Controls");

            const bool carving{ IsCarving(state) };
            ImGui::BeginDisabled(carving);  // the image and settings of a running carve cannot change

            ImGui::SeparatorText("Image");
            const bool hasImage{ !state.original.empty() };
            const ImGuiStyle& style{ ImGui::GetStyle() };
            const float loadWidth{ ImGui::CalcTextSize("Load").x + style.FramePadding.x * 2.0f };
            const float removeWidth{ hasImage ? ImGui::CalcTextSize("Remove").x + style.FramePadding.x * 2.0f + style.ItemSpacing.x : 0.0f };
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - loadWidth - style.ItemSpacing.x - removeWidth);
            const bool enterPressed{ ImGui::InputTextWithHint("##path", "Path to an image...", state.pathBuffer, sizeof(state.pathBuffer),
                                                              ImGuiInputTextFlags_EnterReturnsTrue) };
            ImGui::SameLine();
            if (ImGui::Button("Load") || enterPressed) LoadImage(state);
            if (hasImage) {
                ImGui::SameLine();
                if (ImGui::Button("Remove")) RemoveImage(state);
                ImGui::SetItemTooltip("Remove the image and start over");
            }
            if (!state.underWsl) ImGui::TextDisabled("or drag and drop an image onto the window");

            if (!state.original.empty()) {
                ImGui::SeparatorText("Algorithm");
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::Combo("##algorithm", &state.algorithmIndex, "Dynamic programming\0Greedy\0Graph cut\0");

                DrawEnergyControls(state);
                DrawSizeControls(state);
            }
            ImGui::EndDisabled();

            if (!state.original.empty()) {
                ImGui::Dummy(ImVec2{ 0.0f, ImGui::GetStyle().ItemSpacing.y * 0.5f });

                if (carving) {
                    DrawCarveProgress(state);
                } else {
                    // Nothing to carve when the target is the original size.
                    const bool sameSize{ state.targetWidth == state.original.cols && state.targetHeight == state.original.rows };
                    if (PrimaryButton("Carve", !sameSize)) StartCarve(state);
                    if (sameSize) ImGui::SetItemTooltip("Make the width or height smaller first");
                }

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
            CheckCarveThread(state);

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

        state.cancelCarve = true;  // ask a running carve to stop; it is waited for below

        if (state.originalTexture != 0) glDeleteTextures(1, &state.originalTexture);
        if (state.resultTexture != 0) glDeleteTextures(1, &state.resultTexture);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();

        // The worker uses state.seamsDone / state.cancelCarve, so it must finish before state goes away.
        // Waiting after the window is gone means the app never looks frozen while the carve stops.
        if (state.carveJob.valid()) state.carveJob.wait();
        return 0;
    }

}  // namespace seamcarve
