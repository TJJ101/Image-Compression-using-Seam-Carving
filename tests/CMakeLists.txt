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

add_executable(bench_max_flow bench_max_flow.cpp)
target_link_libraries(bench_max_flow PRIVATE seamcarve_core)
