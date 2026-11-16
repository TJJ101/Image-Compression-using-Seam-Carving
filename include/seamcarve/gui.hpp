#pragma once
#include <string>

namespace seamcarve {
    /**
    * @brief Opens the interactive Dear ImGui window and runs it until it is closed.
    * Only available when the project is built with SEAMCARVE_BUILD_GUI=ON.
    * @param[in] imagePath	- Image to load at startup (empty for none).
    * @return 0 on success, 1 if the window could not be created.
    */
    int RunGui(const std::string& imagePath);

}  // namespace seamcarve
