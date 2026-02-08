#!/bin/bash
# Create test users for ScratchBird security testing
# Uses bootstrap authentication (works when no real users exist)

SB_ADMIN="./build/src/scratchbird-admin"
DB_FILE="$HOME/.scratchbird/testdb/testdb.sdb"

# Check if admin tool exists, if not use SQL via direct connection
if [ ! -f "$SB_ADMIN" ]; then
    echo "Creating users via embedded SQL..."
    
    # Create users using the scratchbird binary with embedded SQL
    # This uses bootstrap authentication
    cat > /tmp/create_users.sql << 'SQLEOF'
-- Create SYSDBA (Firebird-compatible superuser)
CREATE USER IF NOT EXISTS SYSDBA PASSWORD 'SYSDBA2026!' SUPERUSER;

-- Create test users with different roles
CREATE USER IF NOT EXISTS TESTUSER PASSWORD 'TestPass2026!';
CREATE USER IF NOT EXISTS ADMIN PASSWORD 'AdminPass2026!' SUPERUSER;
CREATE USER IF NOT EXISTS APPUSER PASSWORD 'AppPass2026!';

-- Create roles
CREATE ROLE IF NOT EXISTS app_read;
CREATE ROLE IF NOT EXISTS app_write;
CREATE ROLE IF NOT EXISTS app_admin;

-- Grant roles to users
GRANT app_read TO TESTUSER;
GRANT app_write TO TESTUSER;
GRANT app_admin TO ADMIN;

-- Show created users
SELECT user_name, is_superuser FROM system.users;
SQLEOF

    echo "SQL file created. Users need to be created via direct connection."
else
    echo "Using scratchbird-admin..."
fi

echo ""
echo "Test users to be created:"
echo "  SYSDBA    / SYSDBA2026!  (Superuser - Firebird compatible)"
echo "  ADMIN     / AdminPass2026! (Superuser)"
echo "  TESTUSER  / TestPass2026! (Standard user)"
echo "  APPUSER   / AppPass2026! (Application user)"
