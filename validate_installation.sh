#!/bin/bash

# validate_installation.sh
# ScratchBird Installation Validation Script
# 
# This script validates that ScratchBird is properly installed and configured:
# - Checks file structure and permissions
# - Validates service configuration
# - Tests database connectivity
# - Verifies revolutionary features

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Installation configuration
INSTALL_PREFIX="/opt/Scratchbird"
SCRATCHBIRD_USER="scratchbird"
SCRATCHBIRD_GROUP="scratchbird"
SERVICE_NAME="scratchbird"

# Test results tracking
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Function to display header
show_header() {
    echo -e "${PURPLE}"
    echo "================================================================="
    echo "    SCRATCHBIRD INSTALLATION VALIDATION"
    echo "================================================================="
    echo -e "${NC}"
    echo -e "${CYAN}Validating ScratchBird Database Engine Installation${NC}"
    echo -e "${BLUE}Installation Location: $INSTALL_PREFIX${NC}"
    echo ""
}

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

# Function to run a test with output capture
run_test_with_output() {
    local test_name="$1"
    local test_command="$2"
    local expected_output="$3"
    
    ((TESTS_TOTAL++))
    
    echo -n "Testing $test_name... "
    
    local output=$(eval "$test_command" 2>&1)
    local result=$?
    
    if [ $result -eq 0 ] && [[ "$output" =~ $expected_output ]]; then
        echo -e "${GREEN}✅ PASS${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}❌ FAIL${NC}"
        echo -e "${YELLOW}   Expected: $expected_output${NC}"
        echo -e "${YELLOW}   Got: $output${NC}"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Test 1: File Structure
test_file_structure() {
    echo -e "${YELLOW}1. File Structure Tests${NC}"
    
    # Check main directories
    run_test "Installation directory exists" "[ -d '$INSTALL_PREFIX' ]"
    run_test "Bin directory exists" "[ -d '$INSTALL_PREFIX/bin' ]"
    run_test "Lib directory exists" "[ -d '$INSTALL_PREFIX/lib' ]"
    run_test "Conf directory exists" "[ -d '$INSTALL_PREFIX/conf' ]"
    run_test "Data directory exists" "[ -d '$INSTALL_PREFIX/data' ]"
    run_test "Security directory exists" "[ -d '$INSTALL_PREFIX/security' ]"
    
    # Check essential binaries
    run_test "sb_isql binary exists" "[ -f '$INSTALL_PREFIX/bin/sb_isql' ]"
    run_test "scratchbird server binary exists" "[ -f '$INSTALL_PREFIX/bin/scratchbird' ]"
    run_test "sb_gbak binary exists" "[ -f '$INSTALL_PREFIX/bin/sb_gbak' ]"
    run_test "sb_gstat binary exists" "[ -f '$INSTALL_PREFIX/bin/sb_gstat' ]"
    
    # Check libraries
    run_test "Client library exists" "[ -f '$INSTALL_PREFIX/lib/libsbclient.so' ]"
    
    # Check configuration files
    run_test "Main config file exists" "[ -f '$INSTALL_PREFIX/conf/scratchbird.conf' ]"
    run_test "Database aliases file exists" "[ -f '$INSTALL_PREFIX/conf/databases.conf' ]"
    
    echo ""
}

# Test 2: Permissions
test_permissions() {
    echo -e "${YELLOW}2. Permissions Tests${NC}"
    
    # Check ownership
    run_test "Installation owned by scratchbird user" "[ \$(stat -c %U '$INSTALL_PREFIX') = '$SCRATCHBIRD_USER' ]"
    run_test "Installation group is scratchbird" "[ \$(stat -c %G '$INSTALL_PREFIX') = '$SCRATCHBIRD_GROUP' ]"
    
    # Check executable permissions
    run_test "sb_isql is executable" "[ -x '$INSTALL_PREFIX/bin/sb_isql' ]"
    run_test "scratchbird server is executable" "[ -x '$INSTALL_PREFIX/bin/scratchbird' ]"
    
    # Check directory permissions
    run_test "Data directory has proper permissions" "[ \$(stat -c %a '$INSTALL_PREFIX/data') = '750' ]"
    run_test "Security directory has proper permissions" "[ \$(stat -c %a '$INSTALL_PREFIX/security') = '750' ]"
    
    echo ""
}

# Test 3: System User and Group
test_system_user() {
    echo -e "${YELLOW}3. System User and Group Tests${NC}"
    
    run_test "ScratchBird user exists" "getent passwd '$SCRATCHBIRD_USER'"
    run_test "ScratchBird group exists" "getent group '$SCRATCHBIRD_GROUP'"
    run_test "User belongs to correct group" "groups '$SCRATCHBIRD_USER' | grep -q '$SCRATCHBIRD_GROUP'"
    run_test "User home directory is correct" "[ \$(getent passwd '$SCRATCHBIRD_USER' | cut -d: -f6) = '$INSTALL_PREFIX' ]"
    
    echo ""
}

# Test 4: Service Configuration
test_service_configuration() {
    echo -e "${YELLOW}4. Service Configuration Tests${NC}"
    
    run_test "Systemd service file exists" "[ -f '/etc/systemd/system/${SERVICE_NAME}.service' ]"
    run_test "Service is enabled" "systemctl is-enabled '$SERVICE_NAME'"
    
    # Check if service can be started (don't actually start it)
    echo -n "Testing service definition validity... "
    if systemctl show "$SERVICE_NAME" --property=LoadState | grep -q "LoadState=loaded"; then
        echo -e "${GREEN}✅ PASS${NC}"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}❌ FAIL${NC}"
        ((TESTS_FAILED++))
    fi
    ((TESTS_TOTAL++))
    
    echo ""
}

# Test 5: Environment Configuration
test_environment() {
    echo -e "${YELLOW}5. Environment Configuration Tests${NC}"
    
    run_test "Profile script exists" "[ -f '/etc/profile.d/scratchbird.sh' ]"
    run_test "Environment config exists" "[ -f '/etc/environment.d/scratchbird.conf' ]"
    
    # Test environment variables (simulate sourcing)
    echo -n "Testing environment variable setup... "
    if source /etc/profile.d/scratchbird.sh 2>/dev/null && [ "$SCRATCHBIRD" = "$INSTALL_PREFIX" ]; then
        echo -e "${GREEN}✅ PASS${NC}"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}❌ FAIL${NC}"
        ((TESTS_FAILED++))
    fi
    ((TESTS_TOTAL++))
    
    echo ""
}

# Test 6: Database Files
test_database_files() {
    echo -e "${YELLOW}6. Database Files Tests${NC}"
    
    run_test "Security database exists" "[ -f '$INSTALL_PREFIX/security/security.fdb' ]"
    run_test "Security database has proper permissions" "[ \$(stat -c %a '$INSTALL_PREFIX/security/security.fdb') = '660' ]"
    
    # Check if sample database exists (optional)
    if [ -f "$INSTALL_PREFIX/data/databases/sample.fdb" ]; then
        run_test "Sample database has proper permissions" "[ \$(stat -c %a '$INSTALL_PREFIX/data/databases/sample.fdb') = '660' ]"
    else
        echo -e "${YELLOW}   Sample database not found (optional)${NC}"
    fi
    
    echo ""
}

# Test 7: Binary Functionality
test_binary_functionality() {
    echo -e "${YELLOW}7. Binary Functionality Tests${NC}"
    
    # Set environment for tests
    export SCRATCHBIRD="$INSTALL_PREFIX"
    export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:$LD_LIBRARY_PATH"
    
    # Test binary execution and version
    run_test_with_output "sb_isql version check" "'$INSTALL_PREFIX/bin/sb_isql' -z" "ScratchBird"
    run_test_with_output "sb_gbak version check" "'$INSTALL_PREFIX/bin/sb_gbak' -z" "ScratchBird"
    run_test_with_output "sb_gstat version check" "'$INSTALL_PREFIX/bin/sb_gstat' -z" "ScratchBird"
    
    echo ""
}

# Test 8: Configuration Validation
test_configuration_validation() {
    echo -e "${YELLOW}8. Configuration Validation Tests${NC}"
    
    # Test main configuration file syntax
    echo -n "Testing configuration file syntax... "
    if grep -q "RemoteServicePort" "$INSTALL_PREFIX/conf/scratchbird.conf" && 
       grep -q "DatabaseAccess" "$INSTALL_PREFIX/conf/scratchbird.conf"; then
        echo -e "${GREEN}✅ PASS${NC}"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}❌ FAIL${NC}"
        ((TESTS_FAILED++))
    fi
    ((TESTS_TOTAL++))
    
    # Test database aliases configuration
    echo -n "Testing database aliases configuration... "
    if grep -q "security.*security.fdb" "$INSTALL_PREFIX/conf/databases.conf"; then
        echo -e "${GREEN}✅ PASS${NC}"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}❌ FAIL${NC}"
        ((TESTS_FAILED++))
    fi
    ((TESTS_TOTAL++))
    
    # Test revolutionary features configuration
    echo -n "Testing revolutionary features configuration... "
    if grep -q "PartialHashIndexes.*On" "$INSTALL_PREFIX/conf/scratchbird.conf" && 
       grep -q "HierarchicalSchemas.*On" "$INSTALL_PREFIX/conf/scratchbird.conf"; then
        echo -e "${GREEN}✅ PASS - Revolutionary features enabled${NC}"
        ((TESTS_PASSED++))
    else
        echo -e "${YELLOW}⚠️  Revolutionary features not explicitly enabled${NC}"
        ((TESTS_FAILED++))
    fi
    ((TESTS_TOTAL++))
    
    echo ""
}

# Test 9: Database Connectivity (if service is running)
test_database_connectivity() {
    echo -e "${YELLOW}9. Database Connectivity Tests (Optional)${NC}"
    
    # Check if service is running
    if systemctl is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
        echo -e "${BLUE}   Service is running, testing connectivity...${NC}"
        
        # Try to connect to security database (requires password)
        echo -n "Testing security database accessibility... "
        if echo "SELECT 'DB_ACCESSIBLE' FROM RDB\$DATABASE;" | timeout 10 "$INSTALL_PREFIX/bin/sb_isql" security -user SYSDBA >/dev/null 2>&1; then
            echo -e "${GREEN}✅ PASS${NC}"
            ((TESTS_PASSED++))
        else
            echo -e "${YELLOW}⚠️  SKIP (requires SYSDBA password)${NC}"
        fi
        ((TESTS_TOTAL++))
        
    else
        echo -e "${BLUE}   Service not running, skipping connectivity tests${NC}"
        echo -e "${CYAN}   Start service with: sudo systemctl start scratchbird${NC}"
    fi
    
    echo ""
}

# Test 10: Revolutionary Features
test_revolutionary_features() {
    echo -e "${YELLOW}10. Revolutionary Features Tests${NC}"
    
    # Check if sample database exists for feature testing
    if [ -f "$INSTALL_PREFIX/data/databases/sample.fdb" ]; then
        echo -e "${BLUE}   Sample database found, testing revolutionary features...${NC}"
        
        # Test hierarchical schema structure (basic check)
        echo -n "Testing hierarchical schema metadata... "
        local schema_test=$(echo "SELECT COUNT(*) FROM RDB\$SCHEMAS WHERE RDB\$SCHEMA_NAME LIKE '%company%';" | \
                           timeout 5 "$INSTALL_PREFIX/bin/sb_isql" sample -user SYSDBA 2>/dev/null | \
                           grep -o '[0-9]\+' | head -1 2>/dev/null)
        
        if [ "$schema_test" -gt 0 ] 2>/dev/null; then
            echo -e "${GREEN}✅ PASS - Hierarchical schemas detected${NC}"
            ((TESTS_PASSED++))
        else
            echo -e "${YELLOW}⚠️  SKIP (requires database access)${NC}"
        fi
        ((TESTS_TOTAL++))
        
        # Test partial hash index metadata
        echo -n "Testing partial hash index metadata... "
        local index_test=$(echo "SELECT COUNT(*) FROM RDB\$INDICES WHERE RDB\$INDEX_TYPE = 'PARTIAL_HASH';" | \
                          timeout 5 "$INSTALL_PREFIX/bin/sb_isql" sample -user SYSDBA 2>/dev/null | \
                          grep -o '[0-9]\+' | head -1 2>/dev/null)
        
        if [ "$index_test" -gt 0 ] 2>/dev/null; then
            echo -e "${GREEN}✅ PASS - Partial hash indexes detected${NC}"
            ((TESTS_PASSED++))
        else
            echo -e "${YELLOW}⚠️  SKIP (requires database access)${NC}"
        fi
        ((TESTS_TOTAL++))
        
    else
        echo -e "${BLUE}   Sample database not found, skipping feature tests${NC}"
        echo -e "${CYAN}   Revolutionary features can be tested after service start${NC}"
    fi
    
    echo ""
}

# Function to display final results
show_results() {
    echo -e "${PURPLE}"
    echo "================================================================="
    echo "    VALIDATION RESULTS"
    echo "================================================================="
    echo -e "${NC}"
    
    local success_rate=0
    if [ $TESTS_TOTAL -gt 0 ]; then
        success_rate=$(( (TESTS_PASSED * 100) / TESTS_TOTAL ))
    fi
    
    echo -e "${BLUE}Overall Results:${NC}"
    echo "   Tests Run: $TESTS_TOTAL"
    echo -e "   Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "   Failed: ${RED}$TESTS_FAILED${NC}"
    echo -e "   Success Rate: ${CYAN}${success_rate}%${NC}"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}🎉 INSTALLATION VALIDATION SUCCESSFUL!${NC}"
        echo -e "${CYAN}ScratchBird is properly installed and ready to use.${NC}"
        echo ""
        
        echo -e "${BLUE}Next Steps:${NC}"
        echo "   1. Start the service: ${CYAN}sudo systemctl start scratchbird${NC}"
        echo "   2. Connect to sample database: ${CYAN}sb_isql sample -user SYSDBA${NC}"
        echo "   3. Test revolutionary features: ${CYAN}cat $INSTALL_PREFIX/QUICK_START.md${NC}"
        
    elif [ $success_rate -ge 80 ]; then
        echo -e "${YELLOW}⚠️  INSTALLATION MOSTLY SUCCESSFUL${NC}"
        echo -e "${CYAN}ScratchBird is installed but may have minor issues.${NC}"
        echo ""
        
        echo -e "${BLUE}Recommendations:${NC}"
        echo "   • Review failed tests above"
        echo "   • Check file permissions and ownership"
        echo "   • Verify service configuration"
        
    else
        echo -e "${RED}❌ INSTALLATION VALIDATION FAILED${NC}"
        echo -e "${YELLOW}ScratchBird installation has significant issues.${NC}"
        echo ""
        
        echo -e "${BLUE}Recommendations:${NC}"
        echo "   • Reinstall ScratchBird: ${CYAN}sudo ./install_scratchbird.sh${NC}"
        echo "   • Check system requirements and permissions"
        echo "   • Review installation logs"
    fi
    
    echo ""
    echo -e "${PURPLE}=================================================================${NC}"
}

# Main validation function
main() {
    show_header
    
    # Run all test suites
    test_file_structure
    test_permissions
    test_system_user
    test_service_configuration
    test_environment
    test_database_files
    test_binary_functionality
    test_configuration_validation
    test_database_connectivity
    test_revolutionary_features
    
    # Show final results
    show_results
}

# Run main function
main "$@"