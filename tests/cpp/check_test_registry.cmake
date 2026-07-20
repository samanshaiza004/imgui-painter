if(NOT DEFINED TEST_EXECUTABLE OR NOT DEFINED EXPECTED_TESTS_CSV)
    message(FATAL_ERROR "TEST_EXECUTABLE and EXPECTED_TESTS_CSV are required")
endif()

execute_process(
    COMMAND "${TEST_EXECUTABLE}" --list
    RESULT_VARIABLE list_result
    OUTPUT_VARIABLE registered_output
    ERROR_VARIABLE list_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "${TEST_EXECUTABLE} --list failed: ${list_error}")
endif()

string(REPLACE "\n" ";" registered_tests "${registered_output}")
string(REPLACE "," ";" expected_tests "${EXPECTED_TESTS_CSV}")
list(SORT registered_tests)
list(SORT expected_tests)

if(NOT registered_tests STREQUAL expected_tests)
    message(FATAL_ERROR
        "CMake test list and binary registry differ.\n"
        "CMake: ${expected_tests}\n"
        "Binary: ${registered_tests}")
endif()
