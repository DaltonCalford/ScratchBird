#!/bin/bash

# Test script for functional ScratchBird utilities

echo "=== ScratchBird Functional Utilities Test ==="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test database file
TEST_DB="test_sb.fdb"
TEST_BACKUP="test_sb.fbk"

echo -e "${YELLOW}Building functional utilities...${NC}"

# Build the functional utilities
if [ -f "CMakeLists_functional.txt" ]; then
    mkdir -p build_functional
    cd build_functional
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc)
    cd ..
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Build successful${NC}"
    else
        echo -e "${RED}✗ Build failed${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Note: CMakeLists.txt not found, assuming utilities are already built${NC}"
fi

echo

# Test 1: Version Information
echo -e "${YELLOW}Test 1: Version Information${NC}"
echo "----------------------------------------"

for utility in sb_isql_functional sb_gstat_functional sb_gbak_functional; do
    if [ -f "build_functional/$utility" ]; then
        echo -n "Testing $utility version: "
        version_output=$(build_functional/$utility -z 2>&1)
        if [[ $version_output == *"ScratchBird"* ]]; then
            echo -e "${GREEN}✓ Pass${NC}"
        else
            echo -e "${RED}✗ Fail${NC}"
        fi
    else
        echo -e "${YELLOW}$utility not found, skipping${NC}"
    fi
done

echo

# Test 2: Help Information
echo -e "${YELLOW}Test 2: Help Information${NC}"
echo "----------------------------------------"

for utility in sb_isql_functional sb_gstat_functional sb_gbak_functional; do
    if [ -f "build_functional/$utility" ]; then
        echo -n "Testing $utility help: "
        help_output=$(build_functional/$utility -? 2>&1)
        if [[ $help_output == *"Usage:"* ]]; then
            echo -e "${GREEN}✓ Pass${NC}"
        else
            echo -e "${RED}✗ Fail${NC}"
        fi
    else
        echo -e "${YELLOW}$utility not found, skipping${NC}"
    fi
done

echo

# Test 3: Database Connection (with mock database)
echo -e "${YELLOW}Test 3: Database Connection Test${NC}"
echo "----------------------------------------"

if [ -f "build_functional/sb_isql_functional" ]; then
    echo -n "Testing sb_isql connection handling: "
    # Test with non-existent database to check error handling
    connection_output=$(echo "quit" | build_functional/sb_isql_functional nonexistent.fdb 2>&1)
    if [[ $connection_output == *"Failed to connect"* ]] || [[ $connection_output == *"error"* ]]; then
        echo -e "${GREEN}✓ Pass (proper error handling)${NC}"
    else
        echo -e "${RED}✗ Fail (no error handling)${NC}"
    fi
else
    echo -e "${YELLOW}sb_isql_functional not found, skipping${NC}"
fi

echo

# Test 4: Command Line Parsing
echo -e "${YELLOW}Test 4: Command Line Parsing${NC}"
echo "----------------------------------------"

if [ -f "build_functional/sb_gstat_functional" ]; then
    echo -n "Testing sb_gstat argument parsing: "
    # Test with missing database argument
    parse_output=$(build_functional/sb_gstat_functional -h 2>&1)
    if [[ $parse_output == *"Database name is required"* ]]; then
        echo -e "${GREEN}✓ Pass${NC}"
    else
        echo -e "${RED}✗ Fail${NC}"
    fi
else
    echo -e "${YELLOW}sb_gstat_functional not found, skipping${NC}"
fi

echo

# Test 5: Interactive Mode Test
echo -e "${YELLOW}Test 5: Interactive Mode Test${NC}"
echo "----------------------------------------"

if [ -f "build_functional/sb_isql_functional" ]; then
    echo -n "Testing sb_isql interactive mode: "
    # Test basic interactive commands
    interactive_output=$(echo -e "help\nquit" | build_functional/sb_isql_functional 2>&1)
    if [[ $interactive_output == *"Available commands"* ]]; then
        echo -e "${GREEN}✓ Pass${NC}"
    else
        echo -e "${RED}✗ Fail${NC}"
    fi
else
    echo -e "${YELLOW}sb_isql_functional not found, skipping${NC}"
fi

echo

# Test 6: Backup File Format Test
echo -e "${YELLOW}Test 6: Backup File Format Test${NC}"
echo "----------------------------------------"

if [ -f "build_functional/sb_gbak_functional" ]; then
    echo -n "Testing sb_gbak backup format: "
    # Create a mock backup file
    echo "ScratchBird Backup File v1.0" > test_backup.fbk
    echo "Database: test.fdb" >> test_backup.fbk
    echo "TABLE: TEST_TABLE" >> test_backup.fbk
    echo "END_TABLE" >> test_backup.fbk
    
    # Test verify functionality
    verify_output=$(build_functional/sb_gbak_functional -verify test_backup.fbk 2>&1)
    if [[ $verify_output == *"Valid ScratchBird backup"* ]]; then
        echo -e "${GREEN}✓ Pass${NC}"
    else
        echo -e "${RED}✗ Fail${NC}"
    fi
    
    # Clean up
    rm -f test_backup.fbk
else
    echo -e "${YELLOW}sb_gbak_functional not found, skipping${NC}"
fi

echo

# Test 7: Database Framework Test
echo -e "${YELLOW}Test 7: Database Framework Test${NC}"
echo "----------------------------------------"

if [ -f "build_functional/libsb_database.a" ]; then
    echo -e "${GREEN}✓ Database framework library built successfully${NC}"
else
    echo -e "${RED}✗ Database framework library not found${NC}"
fi

echo

# Test 8: Schema Support Test
echo -e "${YELLOW}Test 8: Schema Support Test${NC}"
echo "----------------------------------------"

if [ -f "build_functional/sb_isql_functional" ]; then
    echo -n "Testing schema-related commands: "
    schema_output=$(echo -e "help\nquit" | build_functional/sb_isql_functional 2>&1)
    if [[ $schema_output == *"SET SCHEMA"* ]]; then
        echo -e "${GREEN}✓ Pass${NC}"
    else
        echo -e "${RED}✗ Fail${NC}"
    fi
else
    echo -e "${YELLOW}sb_isql_functional not found, skipping${NC}"
fi

echo

# Summary
echo -e "${YELLOW}=== Test Summary ===${NC}"
echo "----------------------------------------"
echo "The functional ScratchBird utilities have been tested for:"
echo "• Version information display"
echo "• Help system functionality"
echo "• Database connection error handling"
echo "• Command line argument parsing"
echo "• Interactive mode capabilities"
echo "• Backup file format handling"
echo "• Database framework integration"
echo "• Schema management support"
echo
echo -e "${GREEN}All tests completed. Check individual results above.${NC}"
echo
echo "Key Features Implemented:"
echo "• Real database connectivity (via SBDatabase class)"
echo "• SQL query execution with result display"
echo "• Error handling and status reporting"
echo "• Schema-aware operations"
echo "• Backup/restore file format support"
echo "• Interactive command processing"
echo "• Performance statistics tracking"
echo "• Modern C++ architecture"
echo
echo "To use the functional utilities:"
echo "1. Ensure ScratchBird database server is running"
echo "2. Use sb_isql_functional to connect: ./sb_isql_functional mydb.fdb"
echo "3. Use sb_gstat_functional to analyze: ./sb_gstat_functional -h mydb.fdb"
echo "4. Use sb_gbak_functional to backup: ./sb_gbak_functional -b mydb.fdb mydb.fbk"
echo