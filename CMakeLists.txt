cmake_minimum_required(VERSION 3.20)

project(SeamCarve
    VERSION 0.1.0
    DESCRIPTION "Image compression / content-aware resizing by seam carving"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep the Visual Studio solution tidy: put the automatic ALL_BUILD / ZERO_CHECK projects in a
# folder and don't generate the INSTALL project (this project has no install rules).
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SKIP_INSTALL_RULES ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# OpenCV is used ONLY for image I/O, basic matrix storage and (optionally) Sobel.
# Do NOT use any OpenCV content-aware resizing function (assignment rule).
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

function(seamcarve_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---- Core library (all algorithms live here) --------------------------------
add_library(seamcarve_core STATIC
    src/types.cpp
    src/energy.cpp
    src/seam_dp.cpp
    src/seam_greedy.cpp
    src/seam_graphcut.cpp
    src/seam_carver.cpp
	src/max_flow.cpp
)
target_include_directories(seamcarve_core PUBLIC include)
target_link_libraries(seamcarve_core PUBLIC ${OpenCV_LIBS})
target_include_directories(seamcarve_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
seamcarve_set_warnings(seamcarve_core)

# ---- Executable: CLI, plus the GUI built into the same program -----------------
add_executable(seamcarve src/main.cpp src/cli.cpp)
target_link_libraries(seamcarve PRIVATE seamcarve_core)
seamcarve_set_warnings(seamcarve)
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT seamcarve)  # F5 runs seamcarve

# The GUI (Dear ImGui + GLFW + OpenGL) is compiled into the same executable: run with arguments
# for the CLI, with no arguments or --gui for the window. It needs imgui and glfw3 (installed by
# vcpkg from vcpkg.json). The "default" preset turns it OFF so a plain system OpenCV build (e.g.
# the Ubuntu CI job) does not need those libraries; --gui then reports "no GUI in this build".
option(SEAMCARVE_BUILD_GUI "Build the interactive ImGui/GLFW GUI into seamcarve" ON)
if(SEAMCARVE_BUILD_GUI)
    find_package(imgui CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)

    target_sources(seamcarve PRIVATE src/gui.cpp)
    target_compile_definitions(seamcarve PRIVATE SEAMCARVE_WITH_GUI)
    target_link_libraries(seamcarve PRIVATE imgui::imgui glfw OpenGL::GL)
endif()

# GUI-only build (used for the Windows app, see build.bat): there is no command-line mode, main()
# always opens the window, and with MSVC the program is linked as a GUI app (no console window).
option(SEAMCARVE_GUI_ONLY "Build seamcarve as a GUI-only program (no CLI)" OFF)
if(SEAMCARVE_GUI_ONLY)
    if(NOT SEAMCARVE_BUILD_GUI)
        message(FATAL_ERROR "SEAMCARVE_GUI_ONLY requires SEAMCARVE_BUILD_GUI=ON")
    endif()
    target_compile_definitions(seamcarve PRIVATE SEAMCARVE_GUI_ONLY)
    if(MSVC)
        set_target_properties(seamcarve PROPERTIES WIN32_EXECUTABLE ON)
        target_link_options(seamcarve PRIVATE /ENTRY:mainCRTStartup)  # keep using main()
    endif()
endif()

# Convenience target that builds the CLI and prints its usage:
#   cmake --build --preset <preset> --target seamcarve-help
# (Not named "help": that name is reserved by the Makefile generators.)
if(NOT SEAMCARVE_GUI_ONLY)  # a GUI-only build has no --help
    add_custom_target(seamcarve-help
        COMMAND seamcarve --help
        DEPENDS seamcarve
        COMMENT "seamcarve usage"
        VERBATIM)
endif()

# ---- Tests -------------------------------------------------------------------
# The tests drive the CLI, which a GUI-only build lacks. Skipping include(CTest) there also avoids
# the RUN_TESTS / Nightly / Experimental / Continuous projects in the Visual Studio solution.
if(NOT SEAMCARVE_GUI_ONLY)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
