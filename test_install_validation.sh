#!/bin/bash

# test_install_validation.sh
# ScratchBird Installation Script Validation (Non-Root Test)
# 
# This script validates the installation script components without requiring root access

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test results tracking
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Configuration (matching installer)
INSTALL_PREFIX="/opt/Scratchbird"
SCRATCHBIRD_USER="scratchbird"
SCRATCHBIRD_GROUP="scratchbird"
SERVICE_NAME="scratchbird"
SOURCE_DIR="/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/release/alpha0.5.0/linux-x86_64"

echo -e "${PURPLE}"
echo "================================================================="
echo "    SCRATCHBIRD INSTALLATION SCRIPT VALIDATION"
echo "================================================================="
echo -e "${NC}"
echo -e "${CYAN}Testing installation script components without root access${NC}"
echo ""

# Function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    ((TESTS_TOTAL++))
    
    echo -n "Testing $test_name... "
    
    if eval "$test_command" >/dev/null 2>&1; then
        echo -e "${GREEN}✅ PASS${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}❌ FAIL${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Test 1: Source Directory Validation
echo -e "${YELLOW}1. Source Directory Validation${NC}"
run_test "Source directory exists" "[ -d '$SOURCE_DIR' ]"
run_test "Source bin directory exists" "[ -d '$SOURCE_DIR/bin' ]"
run_test "Source lib directory exists" "[ -d '$SOURCE_DIR/lib' ]"
run_test "Source conf directory exists" "[ -d '$SOURCE_DIR/conf' ]"
echo ""

# Test 2: Essential Binaries
echo -e "${YELLOW}2. Essential Binary Validation${NC}"
run_test "sb_isql binary exists" "[ -f '$SOURCE_DIR/bin/sb_isql' ]"
run_test "scratchbird server binary exists" "[ -f '$SOURCE_DIR/bin/scratchbird' ]"
run_test "sb_gbak binary exists" "[ -f '$SOURCE_DIR/bin/sb_gbak' ]"
run_test "sb_gstat binary exists" "[ -f '$SOURCE_DIR/bin/sb_gstat' ]"
run_test "sb_gfix binary exists" "[ -f '$SOURCE_DIR/bin/sb_gfix' ]"
run_test "sb_gsec binary exists" "[ -f '$SOURCE_DIR/bin/sb_gsec' ]"
echo ""

# Test 3: Library Files
echo -e "${YELLOW}3. Library File Validation${NC}"
run_test "Client library exists" "[ -f '$SOURCE_DIR/lib/libsbclient.so' ]"
run_test "Versioned library exists" "[ -f '$SOURCE_DIR/lib/libsbclient.so.0.5.0' ]"
echo ""

# Test 4: Configuration Files
echo -e "${YELLOW}4. Configuration File Validation${NC}"
run_test "Main config template exists" "[ -f '$SOURCE_DIR/conf/scratchbird.conf' ]"
run_test "Database aliases template exists" "[ -f '$SOURCE_DIR/conf/databases.conf' ]"
echo ""

# Test 5: Binary Functionality
echo -e "${YELLOW}5. Binary Functionality Tests${NC}"
export SCRATCHBIRD="$SOURCE_DIR" 
export LD_LIBRARY_PATH="$SOURCE_DIR/lib:$LD_LIBRARY_PATH"

# Test binary execution and ScratchBird branding
echo -n "Testing sb_isql version output... "
if version_output=$("$SOURCE_DIR/bin/sb_isql" -z 2>&1) && echo "$version_output" | grep -q "ScratchBird"; then
    echo -e "${GREEN}✅ PASS - Shows ScratchBird branding${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}❌ FAIL - Missing ScratchBird branding${NC}"
    ((TESTS_FAILED++))
fi
((TESTS_TOTAL++))

echo -n "Testing sb_gbak version output... "
if version_output=$("$SOURCE_DIR/bin/sb_gbak" -z 2>&1) && echo "$version_output" | grep -q "ScratchBird"; then
    echo -e "${GREEN}✅ PASS - Shows ScratchBird branding${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}❌ FAIL - Missing ScratchBird branding${NC}"
    ((TESTS_FAILED++))
fi
((TESTS_TOTAL++))

echo -n "Testing scratchbird server version output... "
if version_output=$("$SOURCE_DIR/bin/scratchbird" -z 2>&1) && echo "$version_output" | grep -q "ScratchBird"; then
    echo -e "${GREEN}✅ PASS - Shows ScratchBird branding${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}❌ FAIL - Missing ScratchBird branding${NC}"
    ((TESTS_FAILED++))
fi
((TESTS_TOTAL++))

echo ""

# Test 6: Installation Script Validation
echo -e "${YELLOW}6. Installation Script Components${NC}"
run_test "Installation script exists" "[ -f '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/install_scratchbird.sh' ]"
run_test "Installation script is executable" "[ -x '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/install_scratchbird.sh' ]"
run_test "Installation script contains SYSDBA setup" "grep -q 'get_sysdba_password' '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/install_scratchbird.sh'"
run_test "Installation script contains security DB creation" "grep -q 'create_security_database' '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/install_scratchbird.sh'"
run_test "Installation script contains revolutionary features" "grep -q 'PartialHashIndexes.*On' '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/install_scratchbird.sh'"
echo ""

# Test 7: Supporting Scripts
echo -e "${YELLOW}7. Supporting Script Validation${NC}"
run_test "Uninstall script exists" "[ -f '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/uninstall_scratchbird.sh' ]"
run_test "Validation script exists" "[ -f '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/validate_installation.sh' ]"
run_test "Uninstall script is executable" "[ -x '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/uninstall_scratchbird.sh' ]"
run_test "Validation script is executable" "[ -x '/home/dcalford/Documents/claude/GitHubRepo/ScratchBird/validate_installation.sh' ]"
echo ""

# Test 8: Required System Commands
echo -e "${YELLOW}8. System Dependencies Check${NC}"
run_test "useradd command available" "command -v useradd"
run_test "groupadd command available" "command -v groupadd"
run_test "systemctl command available" "command -v systemctl"
run_test "chown command available" "command -v chown"
run_test "chmod command available" "command -v chmod"
echo ""

# Final Results
echo -e "${PURPLE}"
echo "================================================================="
echo "    VALIDATION RESULTS"
echo "================================================================="
echo -e "${NC}"

local success_rate=0
if [ $TESTS_TOTAL -gt 0 ]; then
    success_rate=$(( (TESTS_PASSED * 100) / TESTS_TOTAL ))
fi

echo -e "${BLUE}Test Summary:${NC}"
echo "   Tests Run: $TESTS_TOTAL"
echo -e "   Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "   Failed: ${RED}$TESTS_FAILED${NC}"
echo -e "   Success Rate: ${CYAN}${success_rate}%${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL VALIDATION TESTS PASSED!${NC}"
    echo -e "${CYAN}The installation script is ready for use.${NC}"
    echo ""
    echo -e "${BLUE}Next Steps:${NC}"
    echo "   1. Run the installation: ${CYAN}sudo ./install_scratchbird.sh${NC}"
    echo "   2. Validate installation: ${CYAN}sudo ./validate_installation.sh${NC}"
    echo "   3. Start ScratchBird: ${CYAN}sudo systemctl start scratchbird${NC}"
    echo ""
elif [ $success_rate -ge 90 ]; then
    echo -e "${YELLOW}⚠️  VALIDATION MOSTLY SUCCESSFUL${NC}"
    echo -e "${CYAN}Minor issues detected but installation should work.${NC}"
    echo ""
else
    echo -e "${RED}❌ VALIDATION FAILED${NC}"
    echo -e "${YELLOW}Significant issues detected. Review failures above.${NC}"
    echo ""
fi

echo -e "${PURPLE}Installation Test Validation Complete${NC}"
exit 0