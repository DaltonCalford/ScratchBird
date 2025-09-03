# Test Suite

# Find or fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Test sources
file(GLOB TEST_SOURCES "*.cpp" "unit/*.cpp" "integration/*.cpp")

# Test executable
add_executable(scratchbird_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(scratchbird_tests
    scratchbird_core
    scratchbird_parser
    scratchbird_sblr
    GTest::gtest
    GTest::gtest_main
)

# Register tests
include(GoogleTest)
gtest_discover_tests(scratchbird_tests)

# Explicit add_test for validator
add_test(NAME scratchbird_tests COMMAND scratchbird_tests)
