// Interactive front end (Dear ImGui + GLFW + OpenGL), compiled into the main executable.
// This is only the window/UI scaffolding: load an image, pick an algorithm, choose a target size
// and press "Carve". The seam carving itself lives in the core library; until it is implemented
// the status line shows its error.
#include "seamcarve/gui.hpp"
#include <cstdint>
#include <exception>
#include <iostream>

#include <GLFW/glfw3.h>
#include <imgui.h>
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
            std::string status;
            bool statusIsError{ false };
        };

        /**
        * @brief Prints GLFW errors to stderr.
        * @param[in] error	        - GLFW error code.
        * @param[in] description    - Human-readable description.
        */
        void GlfwErrorCallback(int error, const char* description) {
            std::cerr << "GLFW error " << error << ": " << description << "\n";
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
        * @brief Loads an image from disk and resets the size sliders to its dimensions.
        * @param[in,out] state	- UI state to update (status line is set on failure).
        */
        void LoadImage(GuiState& state) {
            const cv::Mat image{ cv::imread(state.pathBuffer, cv::IMREAD_COLOR) };
            if (image.empty()) {
                state.status = std::string("cannot read image '") + state.pathBuffer + "'";
                state.statusIsError = true;
                return;
            }

            state.original = image;
            state.result.release();
            state.targetWidth = image.cols;
            state.targetHeight = image.rows;
            UploadTexture(state.original, state.originalTexture);
            state.status = "loaded " + std::to_string(image.cols) + "x" + std::to_string(image.rows);
            state.statusIsError = false;
        }

        /**
        * @brief Runs seam carving with the selected algorithm and target size.
        * @param[in,out] state	- UI state; the result texture and status line are updated.
        */
        void RunCarve(GuiState& state) {
            CarveOptions options{};
            options.algorithm = static_cast<Algorithm>(state.algorithmIndex);

            try {
                state.result = Carve(state.original, state.targetWidth, state.targetHeight, options);
                UploadTexture(state.result, state.resultTexture);
                state.status = "carved to " + std::to_string(state.result.cols) + "x" + std::to_string(state.result.rows);
                state.statusIsError = false;
            } catch (const std::exception& e) {
                state.status = std::string("error: ") + e.what();
                state.statusIsError = true;
            }
        }

        /**
        * @brief Draws the control panel (image path, algorithm, size sliders, buttons, status).
        * @param[in,out] state	- UI state edited by the widgets.
        */
        void DrawControls(GuiState& state) {
            ImGui::Begin("Controls");

            ImGui::InputText("Image path", state.pathBuffer, sizeof(state.pathBuffer));
            if (ImGui::Button("Load")) LoadImage(state);

            if (!state.original.empty()) {
                ImGui::Separator();
                ImGui::Combo("Algorithm", &state.algorithmIndex, "Dynamic programming\0Greedy\0Graph cut\0");
                ImGui::SliderInt("Width", &state.targetWidth, 1, state.original.cols);
                ImGui::SliderInt("Height", &state.targetHeight, 1, state.original.rows);
                if (ImGui::Button("Carve")) RunCarve(state);
            }

            if (!state.status.empty()) {
                const ImVec4 color{ state.statusIsError ? ImVec4{ 1.0f, 0.4f, 0.4f, 1.0f } : ImVec4{ 0.6f, 1.0f, 0.6f, 1.0f } };
                ImGui::TextColored(color, "%s", state.status.c_str());
            }

            ImGui::End();
        }

        /**
        * @brief Draws the original and the carved image in their own windows.
        * @param[in] state	- UI state holding the images and textures.
        */
        void DrawImages(const GuiState& state) {
            if (!state.original.empty()) {
                ImGui::Begin("Original");
                ImGui::Image((ImTextureID)(std::intptr_t)state.originalTexture,
                             ImVec2{ static_cast<float>(state.original.cols), static_cast<float>(state.original.rows) });
                ImGui::End();
            }

            if (!state.result.empty()) {
                ImGui::Begin("Result");
                ImGui::Image((ImTextureID)(std::intptr_t)state.resultTexture,
                             ImVec2{ static_cast<float>(state.result.cols), static_cast<float>(state.result.rows) });
                ImGui::End();
            }
        }
    }  // namespace

    int RunGui(const std::string& imagePath) {
        glfwSetErrorCallback(GlfwErrorCallback);
        if (!glfwInit()) return 1;

        const char* const glslVersion{ "#version 130" };
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

        GLFWwindow* const window{ glfwCreateWindow(1280, 800, "Seam Carving", nullptr, nullptr) };
        if (!window) {
            glfwTerminate();
            return 1;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glslVersion);

        GuiState state;
        if (!imagePath.empty()) {
            imagePath.copy(state.pathBuffer, sizeof(state.pathBuffer) - 1);
            LoadImage(state);
        }

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            DrawControls(state);
            DrawImages(state);

            ImGui::Render();
            int framebufferWidth{}, framebufferHeight{};
            glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
            glViewport(0, 0, framebufferWidth, framebufferHeight);
            glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
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