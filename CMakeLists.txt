cmake_minimum_required(VERSION 3.20)

project(SeamCarve
    VERSION 0.1.0
    DESCRIPTION "Image compression / content-aware resizing by seam carving"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
)
target_include_directories(seamcarve_core PUBLIC include)
target_link_libraries(seamcarve_core PUBLIC ${OpenCV_LIBS})
target_include_directories(seamcarve_core SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
seamcarve_set_warnings(seamcarve_core)

# ---- CLI executable ----------------------------------------------------------
add_executable(seamcarve src/main.cpp src/cli.cpp)
target_link_libraries(seamcarve PRIVATE seamcarve_core)
seamcarve_set_warnings(seamcarve)

# Convenience target that builds the CLI and prints its usage:
#   cmake --build --preset <preset> --target seamcarve-help
# (Not named "help": that name is reserved by the Makefile generators.)
add_custom_target(seamcarve-help
    COMMAND seamcarve --help
    DEPENDS seamcarve
    COMMENT "seamcarve usage"
    VERBATIM)

# ---- Tests -------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
