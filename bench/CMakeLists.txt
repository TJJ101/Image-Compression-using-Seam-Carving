# Google Benchmark suite comparing the seam-carving implementations (see bench_seams.cpp).
# Built when SEAMCARVE_BUILD_BENCHMARKS is ON (the "vcpkg" preset turns it on).
add_executable(seamcarve_bench bench_seams.cpp)
target_link_libraries(seamcarve_bench PRIVATE seamcarve_core benchmark::benchmark)
# Sample images used by the Carve benchmarks
target_compile_definitions(seamcarve_bench PRIVATE SEAMCARVE_DATA_DIR="${CMAKE_SOURCE_DIR}/data/input")
seamcarve_set_warnings(seamcarve_bench)
