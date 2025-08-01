#!/bin/bash
#
# Simple Hierarchical Schema Test for ScratchBird v0.6.0
# Tests basic schema creation functionality
#

set -e

# Configuration
SCRATCHBIRD_HOME="gen/Release/scratchbird"
TEST_DB="/tmp/simple_schema_test.fdb"
ISQL_BIN="$SCRATCHBIRD_HOME/bin/sb_isql"

echo "=== Simple Hierarchical Schema Test ==="
echo "Using: $ISQL_BIN"

# Set environment
export SCRATCHBIRD="$SCRATCHBIRD_HOME"

# Clean up any existing test database
rm -f "$TEST_DB"

echo "Step 1: Creating test database..."
echo "CREATE DATABASE '$TEST_DB';" | "$ISQL_BIN" -q 

if [ $? -eq 0 ]; then
    echo "✅ Database creation successful"
else
    echo "❌ Database creation failed"
    exit 1
fi

echo ""
echo "Step 2: Testing basic schema operations..."

# Create a simple test script
cat > /tmp/schema_test.sql << 'EOF'
CONNECT '/tmp/simple_schema_test.fdb';

-- Test 1: Check if SYSTEM schema exists
SELECT 'Test 1: SYSTEM schema check' AS test_description;
SELECT COUNT(*) AS system_schema_count FROM RDB$SCHEMAS WHERE RDB$SCHEMA_NAME = 'SYSTEM';

-- Test 2: Create a simple schema
SELECT 'Test 2: Creating test schema' AS test_description;
CREATE SCHEMA testschema;

-- Test 3: Verify schema was created
SELECT 'Test 3: Verifying schema creation' AS test_description;
SELECT RDB$SCHEMA_NAME FROM RDB$SCHEMAS WHERE RDB$SCHEMA_NAME = 'testschema';

-- Test 4: Create table in schema
SELECT 'Test 4: Creating table in schema' AS test_description;
CREATE TABLE testschema.testtable (id INTEGER, name VARCHAR(50));

-- Test 5: Insert and query data
SELECT 'Test 5: Testing data operations' AS test_description;
INSERT INTO testschema.testtable VALUES (1, 'Test Data');
SELECT * FROM testschema.testtable;

-- Cleanup
DROP TABLE testschema.testtable;
DROP SCHEMA testschema;

SELECT 'Schema tests completed successfully' AS final_result;
QUIT;
EOF

echo "Running schema functionality tests..."
if "$ISQL_BIN" -i /tmp/schema_test.sql > /tmp/schema_test_output.txt 2>&1; then
    echo "✅ Schema tests completed"
    echo ""
    echo "Test Results:"
    cat /tmp/schema_test_output.txt | grep -E "(Test [0-9]|system_schema_count|testschema|Test Data|completed successfully)"
else
    echo "❌ Schema tests failed"
    echo "Error output:"
    cat /tmp/schema_test_output.txt
    exit 1
fi

# Cleanup
rm -f "$TEST_DB" /tmp/schema_test.sql /tmp/schema_test_output.txt

echo ""
echo "🎉 Simple schema test completed successfully!"
echo "✅ Basic hierarchical schema functionality is working"