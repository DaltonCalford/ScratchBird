#!/bin/bash

# demo_configuration.sh
# Demonstration of ScratchBird test configuration flexibility
# Shows how to customize test environment without modifying scripts

echo "========================================================"
echo "SCRATCHBIRD TEST CONFIGURATION DEMONSTRATION"
echo "========================================================"
echo

echo "1. DEFAULT CONFIGURATION:"
echo "========================="
source ./test_config.sh >/dev/null 2>&1
echo "✓ Database Location: $SB_TEST_DB_LOCATION"
echo "✓ User: $SB_TEST_USER"
echo "✓ Results Directory: $SB_TEST_RESULTS_DIR"
echo "✓ Database Directory: $SB_TEST_DB_DIR"
echo

echo "2. TEMPORARY DATABASE CONFIGURATION:"
echo "===================================="
export SB_TEST_DB_LOCATION="temp"
export SB_TEST_CLEANUP="true"
export SB_TEST_VERBOSE="false"
source ./test_config.sh >/dev/null 2>&1
echo "✓ Database Location: $SB_TEST_DB_LOCATION"
echo "✓ Database Path Example: $(generate_db_path 'test' 'sample')"
echo "✓ Cleanup Enabled: $SB_TEST_CLEANUP"
echo

echo "3. REMOTE SERVER CONFIGURATION:"
echo "==============================="
export SB_TEST_DB_LOCATION="remote"
export SB_TEST_SERVER="database-server.company.com"
export SB_TEST_PORT="3050"
export SB_TEST_USER="testuser"
export SB_TEST_PASSWORD="secure_password"
source ./test_config.sh >/dev/null 2>&1
echo "✓ Database Location: $SB_TEST_DB_LOCATION"
echo "✓ Server: $SB_TEST_SERVER:$SB_TEST_PORT"
echo "✓ User: $SB_TEST_USER"
echo "✓ Database Path Example: $(generate_db_path 'test' 'sample')"
echo "✓ Connection String: $(generate_connection_string $(generate_db_path 'test' 'sample'))"
echo

echo "4. CUSTOM INSTALLATION DIRECTORY:"
echo "================================="
export SB_TEST_DB_LOCATION="local"
export SB_INSTALL_DIR="/opt/scratchbird-custom"
export SB_TEST_BASE_DIR="/custom/test/location"
source ./test_config.sh >/dev/null 2>&1
echo "✓ Installation Directory: $SB_INSTALL_DIR"
echo "✓ Test Base Directory: $SB_TEST_BASE_DIR"
echo "✓ sb_isql Path: $SB_ISQL"
echo

echo "5. DEVELOPMENT/DEBUG CONFIGURATION:"
echo "==================================="
export SB_TEST_DB_LOCATION="local"
export SB_TEST_VERBOSE="true"
export SB_TEST_STOP_ON_ERROR="true"
export SB_TEST_CLEANUP="false"
source ./test_config.sh >/dev/null 2>&1
echo "✓ Verbose Mode: $SB_TEST_VERBOSE"
echo "✓ Stop on Error: $SB_TEST_STOP_ON_ERROR"
echo "✓ Cleanup Disabled: $SB_TEST_CLEANUP (databases preserved for debugging)"
echo

echo "6. CONFIGURATION VALIDATION:"
echo "============================"
if validate_test_config >/dev/null 2>&1; then
    echo "✅ Configuration validation: PASSED"
else
    echo "❌ Configuration validation: FAILED"
fi
echo

echo "7. SAMPLE ENVIRONMENT FILE (.env):"
echo "=================================="
cat << 'EOF'
# Save this as .env in your test directory
SB_TEST_USER=your_username
SB_TEST_PASSWORD=your_password
SB_TEST_DB_LOCATION=local
SB_TEST_VERBOSE=false
SB_INSTALL_DIR=/path/to/scratchbird
SB_TEST_CLEANUP=true

# Then run: source .env && ./run_all_tests.sh
EOF

echo
echo "8. USAGE EXAMPLES:"
echo "=================="
echo "# Quick local testing (default):"
echo "./run_all_tests.sh"
echo
echo "# Remote server testing:"
echo "export SB_TEST_DB_LOCATION=remote SB_TEST_SERVER=myserver"
echo "./run_all_tests.sh"
echo
echo "# Temporary database testing:"
echo "export SB_TEST_DB_LOCATION=temp"
echo "./run_all_tests.sh"
echo
echo "# Custom installation with verbose output:"
echo "export SB_INSTALL_DIR=/opt/scratchbird SB_TEST_VERBOSE=true"
echo "./run_all_tests.sh"
echo

echo "========================================================"
echo "BENEFITS OF CENTRALIZED CONFIGURATION:"
echo "========================================================"
echo "✅ No hardcoded paths or credentials in test scripts"
echo "✅ Easy customization through environment variables"
echo "✅ Support for local, remote, and temporary databases"
echo "✅ Flexible installation directory support"
echo "✅ Comprehensive logging and validation"
echo "✅ CI/CD and Docker-friendly configuration"
echo "✅ Portable test scripts across environments"
echo

echo "To customize your tests, simply set environment variables"
echo "before running any test script. All scripts now use the"
echo "centralized configuration automatically!"
echo
echo "Run './test_config.sh' to see your current configuration."
echo "========================================================"