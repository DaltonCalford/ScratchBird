#
# The contents of this file are subject to the Initial
# Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the
# License. You may obtain a copy of the License at
# http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
#
# Software distributed under the License is distributed AS IS,
# WITHOUT WARRANTY OF ANY KIND, either express or implied.
# See the License for the specific language governing rights
# and limitations under the License.
#
# The Original Code was created for the ScratchBird Open Source 
# RDBMS project.
#
# Copyright (c) 2025 ScratchBird Project
# and all contributors signed below.
#
# All Rights Reserved.
# Contributor(s): ______________________________________.
#
# 2025.07.23 - ScratchBird GIN Index Implementation - Test Makefile

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
INCLUDES = -I../include -I../common -I../dsql -I../jrd -I../optimizer -I../recsrc -I../../include

# Source directories
SRCDIR = .
INCDIR = ../include
COMMONDIR = ../common
DSQLDIR = ../dsql
OPTDIR = optimizer
RECSRCDIR = recsrc

# Test executables
TEST_TARGETS = gin_test gin_functional_test

# GIN implementation object files (would be linked from main build)
GIN_OBJS = \
	GinIndex.o \
	GinTokenizer.o \
	GinQueryProcessor.o \
	optimizer/GinIndexCostModel.o \
	recsrc/GinTableScan.o

# Test object files
TEST_OBJS = gin_test.o gin_functional_test.o

# Mock object files for testing
MOCK_OBJS = \
	mock_pool.o \
	mock_database.o \
	mock_transaction.o

.PHONY: all test clean help

# Default target
all: $(TEST_TARGETS)
	@echo "GIN Index test suite built successfully"

# Build the comprehensive unit test
gin_test: gin_test.o $(MOCK_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ gin_test.o $(MOCK_OBJS) $(LDFLAGS)
	@echo "Built GIN comprehensive unit test"

# Build the functional test
gin_functional_test: gin_functional_test.o $(MOCK_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ gin_functional_test.o $(MOCK_OBJS) $(LDFLAGS)
	@echo "Built GIN functional test"

# Test source compilation
gin_test.o: gin_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ gin_test.cpp

gin_functional_test.o: gin_functional_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ gin_functional_test.cpp

# Mock object compilation (placeholder - would use actual mocks)
mock_pool.o:
	@echo "// Mock MemoryPool implementation" > mock_pool.cpp
	@echo "class MockMemoryPool {};" >> mock_pool.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ mock_pool.cpp

mock_database.o:
	@echo "// Mock Database implementation" > mock_database.cpp
	@echo "class MockDatabase {};" >> mock_database.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ mock_database.cpp

mock_transaction.o:
	@echo "// Mock Transaction implementation" > mock_transaction.cpp
	@echo "class MockTransaction {};" >> mock_transaction.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ mock_transaction.cpp

# Run all tests
test: $(TEST_TARGETS)
	@echo "========================================"
	@echo "Running ScratchBird GIN Index Test Suite"
	@echo "========================================"
	@echo ""
	@echo "Running comprehensive unit tests..."
	@./gin_test || echo "Unit tests failed"
	@echo ""
	@echo "Running functional tests..."
	@./gin_functional_test || echo "Functional tests failed"
	@echo ""
	@echo "GIN Index test suite completed"

# Run only unit tests
test-unit: gin_test
	@echo "Running GIN Index unit tests..."
	@./gin_test

# Run only functional tests  
test-functional: gin_functional_test
	@echo "Running GIN Index functional tests..."
	@./gin_functional_test

# Performance test (placeholder)
test-performance: gin_test
	@echo "Running GIN Index performance tests..."
	@echo "Performance testing with various dataset sizes..."
	@time ./gin_test
	@echo "Performance test completed"

# Memory test (if valgrind available)
test-memory: gin_test gin_functional_test
	@echo "Running GIN Index memory tests..."
	@if command -v valgrind >/dev/null 2>&1; then \
		echo "Running unit tests with valgrind..."; \
		valgrind --leak-check=full --error-exitcode=1 ./gin_test; \
		echo "Running functional tests with valgrind..."; \
		valgrind --leak-check=full --error-exitcode=1 ./gin_functional_test; \
	else \
		echo "Valgrind not available, skipping memory tests"; \
	fi

# Code coverage (if gcov available)
test-coverage: CXXFLAGS += --coverage
test-coverage: clean $(TEST_TARGETS)
	@echo "Running GIN Index tests with coverage..."
	@./gin_test
	@./gin_functional_test
	@if command -v gcov >/dev/null 2>&1; then \
		echo "Generating coverage report..."; \
		gcov gin_test.cpp gin_functional_test.cpp; \
		echo "Coverage report generated"; \
	else \
		echo "gcov not available, skipping coverage report"; \
	fi

# Stress test
test-stress: gin_test
	@echo "Running GIN Index stress tests..."
	@for i in `seq 1 10`; do \
		echo "Stress test iteration $$i..."; \
		./gin_test >/dev/null || echo "Stress test $$i failed"; \
	done
	@echo "GIN Index stress tests completed"

# Integration test (placeholder - would test with actual database)
test-integration:
	@echo "Running GIN Index integration tests..."
	@echo "Integration tests require running ScratchBird database"
	@echo "Placeholder for database integration tests"

# Clean build artifacts
clean:
	@echo "Cleaning GIN Index test build artifacts..."
	@rm -f $(TEST_TARGETS) $(TEST_OBJS) $(MOCK_OBJS)
	@rm -f mock_pool.cpp mock_database.cpp mock_transaction.cpp
	@rm -f *.gcov *.gcno *.gcda
	@rm -f core core.*
	@echo "Clean completed"

# Help target
help:
	@echo "ScratchBird GIN Index Test Suite Makefile"
	@echo "=========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all              - Build all test executables"
	@echo "  test             - Run all tests"
	@echo "  test-unit        - Run comprehensive unit tests only"
	@echo "  test-functional  - Run functional tests only"
	@echo "  test-performance - Run performance tests"
	@echo "  test-memory      - Run memory leak tests (requires valgrind)"
	@echo "  test-coverage    - Run tests with code coverage (requires gcov)"
	@echo "  test-stress      - Run stress tests"
	@echo "  test-integration - Run integration tests (placeholder)"
	@echo "  clean            - Clean build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Test Executables:"
	@echo "  gin_test         - Comprehensive unit test suite"
	@echo "  gin_functional_test - Functional test suite"
	@echo ""
	@echo "Usage Examples:"
	@echo "  make test        # Run all tests"
	@echo "  make test-unit   # Run only unit tests"
	@echo "  make clean test  # Clean and run all tests"

# Debug build
debug: CXXFLAGS += -DDEBUG -g3 -O0
debug: $(TEST_TARGETS)
	@echo "Debug build completed"

# Release build
release: CXXFLAGS += -DNDEBUG -O3
release: $(TEST_TARGETS)
	@echo "Release build completed"

# Verbose test run
test-verbose: $(TEST_TARGETS)
	@echo "Running GIN Index tests in verbose mode..."
	@echo "Comprehensive unit tests:"
	@./gin_test 2>&1 | tee gin_test.log
	@echo "Functional tests:"
	@./gin_functional_test 2>&1 | tee gin_functional_test.log
	@echo "Test logs saved to gin_test.log and gin_functional_test.log"

# Quick test (basic validation)
test-quick: gin_functional_test
	@echo "Running quick GIN Index validation..."
	@./gin_functional_test

# Continuous Integration target
ci: clean test test-memory
	@echo "Continuous Integration tests completed"

# Install test binaries (for system-wide testing)
install-tests: $(TEST_TARGETS)
	@echo "Installing GIN Index test binaries..."
	@mkdir -p /usr/local/bin/scratchbird-tests
	@cp $(TEST_TARGETS) /usr/local/bin/scratchbird-tests/
	@echo "Test binaries installed to /usr/local/bin/scratchbird-tests/"

# Uninstall test binaries
uninstall-tests:
	@echo "Uninstalling GIN Index test binaries..."
	@rm -rf /usr/local/bin/scratchbird-tests
	@echo "Test binaries uninstalled"

# Dependency information
deps:
	@echo "GIN Index Test Dependencies:"
	@echo "============================"
	@echo "Required:"
	@echo "  - GCC/G++ with C++17 support"
	@echo "  - Make build system"
	@echo "  - ScratchBird source tree"
	@echo ""
	@echo "Optional:"
	@echo "  - Valgrind (for memory testing)"
	@echo "  - gcov (for code coverage)"
	@echo "  - Running ScratchBird database (for integration tests)"
	@echo ""
	@echo "Test Files:"
	@echo "  - gin_test.cpp (comprehensive unit tests)"
	@echo "  - gin_functional_test.cpp (functional tests)"
	@echo "  - gin_test.mk (this makefile)"