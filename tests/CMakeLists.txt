# CLI smoke tests. Algorithm unit tests (energy, seams, ...) get added here as they are written.
add_test(NAME cli_help COMMAND seamcarve --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: seamcarve")

add_test(NAME cli_missing_input COMMAND seamcarve --width 10)
set_tests_properties(cli_missing_input PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_bad_algo COMMAND seamcarve -i x.png -a nope -w 10)
set_tests_properties(cli_bad_algo PROPERTIES WILL_FAIL TRUE)

add_executable(test_max_flow test_max_flow.cpp)
target_link_libraries(test_max_flow PRIVATE seamcarve_core)
add_test(NAME max_flow COMMAND test_max_flow)

add_executable(test_seam_graphcut test_seam_graphcut.cpp)
target_link_libraries(test_seam_graphcut PRIVATE seamcarve_core)
add_test(NAME seam_graphcut COMMAND test_seam_graphcut)

add_executable(test_graphcut_vs_dp test_graphcut_vs_dp.cpp)
target_link_libraries(test_graphcut_vs_dp PRIVATE seamcarve_core)
add_test(NAME graphcut_vs_dp COMMAND test_graphcut_vs_dp)
set_tests_properties(graphcut_vs_dp PROPERTIES SKIP_RETURN_CODE 77)

add_executable(bench_max_flow bench_max_flow.cpp)
target_link_libraries(bench_max_flow PRIVATE seamcarve_core)
add_test(NAME cli_bad_energy COMMAND seamcarve -i x.png -e nope -w 10)
set_tests_properties(cli_bad_energy PROPERTIES WILL_FAIL TRUE)

# Divide-and-conquer options. These check the error message, not just that something failed, because
# the image x.png does not exist and would also make the program fail.
add_test(NAME cli_bad_dc_type COMMAND seamcarve -i x.png -e dc -t nope -w 10)
set_tests_properties(cli_bad_dc_type PROPERTIES PASS_REGULAR_EXPRESSION "unknown divide-and-conquer type")

add_test(NAME cli_dc_option_needs_dc COMMAND seamcarve -i x.png -e sobel -b 16 -w 10)
set_tests_properties(cli_dc_option_needs_dc PROPERTIES PASS_REGULAR_EXPRESSION "only apply with --energy dc")

add_test(NAME cli_recursion_base_size_too_small COMMAND seamcarve -i x.png -e dc -t recursion -b 2 -w 10)
set_tests_properties(cli_recursion_base_size_too_small PROPERTIES PASS_REGULAR_EXPRESSION "at least 3")

# Energy unit tests: a separate program linked to the algorithm library. Methods that are still
# stubs are skipped (exit code 77), so this only fails for code that is implemented but wrong.
add_executable(energy_test energy_test.cpp)
target_link_libraries(energy_test PRIVATE seamcarve_core)
seamcarve_set_warnings(energy_test)
# Keep the test program out of build/ (which should only hold seamcarve): put it next to the test files.
set_target_properties(energy_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME energy COMMAND energy_test)
set_tests_properties(energy PROPERTIES SKIP_RETURN_CODE 77)
