// Minimal test runner for ExpressionMatcher and PredicateMatcher tests only
// This bypasses the ASTPrinter issue in other tests

#include <gtest/gtest.h>

// Main function - GoogleTest will discover and run all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
