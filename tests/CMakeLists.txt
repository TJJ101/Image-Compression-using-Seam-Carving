# CLI smoke tests. Algorithm unit tests (energy, seams, ...) get added here as they are written.
add_test(NAME cli_help COMMAND seamcarve --help)
set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage: seamcarve")

add_test(NAME cli_missing_input COMMAND seamcarve --width 10)
set_tests_properties(cli_missing_input PROPERTIES WILL_FAIL TRUE)

add_test(NAME cli_bad_algo COMMAND seamcarve -i x.png -a nope -w 10)
set_tests_properties(cli_bad_algo PROPERTIES WILL_FAIL TRUE)
