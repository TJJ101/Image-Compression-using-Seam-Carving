# CLI smoke tests. Algorithm unit tests (energy, seams, ...) get added here as they are written.
add_test(NAME cli_help COMMAND seamcarve --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: seamcarve")

add_test(NAME cli_missing_input COMMAND seamcarve --width 10)
set_tests_properties(cli_missing_input PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_bad_algo COMMAND seamcarve -i x.png -a nope -w 10)
set_tests_properties(cli_bad_algo PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_bad_energy COMMAND seamcarve -i x.png -e nope -w 10)
set_tests_properties(cli_bad_energy PROPERTIES WILL_FAIL TRUE)

# Energy unit tests: a separate program linked to the algorithm library. Methods that are still
# stubs are skipped (exit code 77), so this only fails for code that is implemented but wrong.
add_executable(energy_test energy_test.cpp)
target_link_libraries(energy_test PRIVATE seamcarve_core)
seamcarve_set_warnings(energy_test)
# Keep the test program out of build/ (which should only hold seamcarve): put it next to the test files.
set_target_properties(energy_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME energy COMMAND energy_test)
set_tests_properties(energy PROPERTIES SKIP_RETURN_CODE 77)
