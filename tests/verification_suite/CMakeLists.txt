# Comprehensive Verification Test Suite for ScratchBird
# This test suite verifies that the database actually works and exposes all issues

cmake_minimum_required(VERSION 3.20)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Define test categories
set(TEST_CATEGORIES
    core
    security
    storage
    concurrency
    compliance
    performance
    tools
    integration
    phases  # Project plan phase-specific tests
)

# Create test executable for each category
foreach(CATEGORY ${TEST_CATEGORIES})
    file(GLOB ${CATEGORY}_TEST_SOURCES "${CATEGORY}/*.cpp")
    
    if(${CATEGORY}_TEST_SOURCES)
        add_executable(test_${CATEGORY}_verification ${${CATEGORY}_TEST_SOURCES})
        
        target_link_libraries(test_${CATEGORY}_verification
            scratchbird_engine
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )
        
        target_include_directories(test_${CATEGORY}_verification PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        )
        
        # Add as CTest
        add_test(NAME verification_${CATEGORY} 
                 COMMAND test_${CATEGORY}_verification
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        
        # Set test properties
        set_tests_properties(verification_${CATEGORY} PROPERTIES
            TIMEOUT 300  # 5 minutes timeout
            LABELS "verification;${CATEGORY}"
        )
    endif()
endforeach()

# Create master test executable that runs all tests
add_executable(test_verification_all
    run_all_tests.cpp
)

target_link_libraries(test_verification_all
    scratchbird_engine
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Create ordered test executable that respects dependencies
add_executable(test_verification_ordered
    run_ordered_tests.cpp
)

target_link_libraries(test_verification_ordered
    scratchbird_engine
    GTest::gtest
    Threads::Threads
)

target_include_directories(test_verification_ordered PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add master test
add_test(NAME verification_all
         COMMAND test_verification_all
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

set_tests_properties(verification_all PROPERTIES
    TIMEOUT 1800  # 30 minutes for all tests
    LABELS "verification;all"
)

# Custom target to run verification suite
add_custom_target(verify
    COMMAND ${CMAKE_CTEST_COMMAND} -L verification --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running comprehensive verification test suite..."
)

# Add performance benchmarks
if(SCRATCHBIRD_PERF)
    add_executable(benchmark_verification
        performance/benchmark_suite.cpp
    )
    
    target_link_libraries(benchmark_verification
        scratchbird_engine
        benchmark::benchmark
        Threads::Threads
    )
    
    add_custom_target(benchmark
        COMMAND benchmark_verification
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running performance benchmarks..."
    )
endif()

# Generate test report
add_custom_target(test_report
    COMMAND ${CMAKE_CTEST_COMMAND} -L verification --output-on-failure --verbose > test_report.txt 2>&1
    COMMAND ${CMAKE_COMMAND} -E echo "Test report generated: test_report.txt"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating comprehensive test report..."
)