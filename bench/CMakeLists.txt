# Google Benchmark suite comparing the seam-carving implementations (see bench_seams.cpp).
# Built when SEAMCARVE_BUILD_BENCHMARKS is ON (the "vcpkg" and "windows-gui" presets turn it on), as
# a separate console program: in the Visual Studio solution it is its own project next to seamcarve.
add_executable(seamcarve_bench bench_seams.cpp)
target_link_libraries(seamcarve_bench PRIVATE seamcarve_core benchmark::benchmark)
# Sample images used by the Carve benchmarks
target_compile_definitions(seamcarve_bench PRIVATE SEAMCARVE_DATA_DIR="${CMAKE_SOURCE_DIR}/data/input")
seamcarve_set_warnings(seamcarve_bench)
# Show bench_seams.cpp under bench/ in Solution Explorer, like the other projects' files
source_group(TREE ${CMAKE_SOURCE_DIR} FILES bench_seams.cpp)
