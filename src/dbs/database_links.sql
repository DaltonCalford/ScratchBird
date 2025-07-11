/*
 * Database Links System Table Schema
 * This file defines the system table for storing database link definitions
 */

-- System table for database links
CREATE TABLE RDB$DATABASE_LINKS (
    RDB$LINK_NAME           CHAR(63) CHARACTER SET UNICODE_FSS NOT NULL,
    RDB$LINK_TARGET         VARCHAR(255) CHARACTER SET UNICODE_FSS NOT NULL,
    RDB$LINK_USER           VARCHAR(63) CHARACTER SET UNICODE_FSS,
    RDB$LINK_PASSWORD       VARCHAR(64) CHARACTER SET UNICODE_FSS,
    RDB$LINK_ROLE           VARCHAR(63) CHARACTER SET UNICODE_FSS,
    RDB$LINK_FLAGS          INTEGER DEFAULT 0 NOT NULL,
    RDB$LINK_PROVIDER       VARCHAR(32) CHARACTER SET UNICODE_FSS,
    RDB$LINK_POOL_MIN       INTEGER DEFAULT 1,
    RDB$LINK_POOL_MAX       INTEGER DEFAULT 10,
    RDB$LINK_TIMEOUT        INTEGER DEFAULT 3600,
    RDB$LINK_CREATED        TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
    RDB$LINK_DESCRIPTION    BLOB SUB_TYPE TEXT CHARACTER SET UNICODE_FSS,
    -- Schema-aware extensions
    RDB$LINK_SCHEMA_NAME    VARCHAR(703) CHARACTER SET UNICODE_FSS,    -- Local schema where link is created
    RDB$LINK_REMOTE_SCHEMA  VARCHAR(703) CHARACTER SET UNICODE_FSS,    -- Remote schema path to target
    RDB$LINK_SCHEMA_MODE    INTEGER DEFAULT 0 NOT NULL,               -- Schema resolution mode
    RDB$LINK_SCHEMA_DEPTH   SMALLINT DEFAULT 0 NOT NULL,              -- Cached schema depth for optimization
    CONSTRAINT PK_DATABASE_LINKS PRIMARY KEY (RDB$LINK_NAME)
);

-- Grant permissions for database link operations
GRANT SELECT ON RDB$DATABASE_LINKS TO PUBLIC;
GRANT INSERT, UPDATE, DELETE ON RDB$DATABASE_LINKS TO RDB$ADMIN;

-- Database link flags
-- Bit 0: Use trusted authentication (0x01)
-- Bit 1: Use session authentication (0x02) 
-- Bit 2: Connection pooling enabled (0x04)
-- Bit 3: Read-only link (0x08)
-- Bit 4: Auto-reconnect enabled (0x10)
-- Bit 5: Schema-aware link (0x20)
-- Bit 6: Inherit schema context (0x40)

-- Schema resolution modes (RDB$LINK_SCHEMA_MODE)
-- 0: No schema awareness (legacy mode)
-- 1: Fixed remote schema (always use RDB$LINK_REMOTE_SCHEMA)
-- 2: Context-aware schema (resolve schema references like HOME, CURRENT)
-- 3: Hierarchical mapping (map local schema hierarchy to remote)
-- 4: Mirror mode (local schema path mirrors remote schema path)

-- Default database links provider
INSERT INTO RDB$DATABASE_LINKS (
    RDB$LINK_NAME, 
    RDB$LINK_TARGET, 
    RDB$LINK_FLAGS, 
    RDB$LINK_PROVIDER,
    RDB$LINK_DESCRIPTION
) VALUES (
    'SELF', 
    'localhost:' || RDB$GET_CONTEXT('SYSTEM', 'DB_NAME'), 
    7, -- trusted + session + pooling
    'Firebird',
    'Self-reference link to current database'
);

-- System indices for performance
CREATE INDEX RDB$IDX_DATABASE_LINKS_TARGET ON RDB$DATABASE_LINKS (RDB$LINK_TARGET);
CREATE INDEX RDB$IDX_DATABASE_LINKS_PROVIDER ON RDB$DATABASE_LINKS (RDB$LINK_PROVIDER);

-- System view for easier querying
CREATE VIEW RDB$DATABASE_LINKS_VIEW AS
SELECT 
    RDB$LINK_NAME AS LINK_NAME,
    RDB$LINK_TARGET AS TARGET_DATABASE,
    CASE 
        WHEN BIN_AND(RDB$LINK_FLAGS, 1) = 1 THEN 'TRUSTED'
        WHEN RDB$LINK_USER IS NOT NULL THEN 'EXPLICIT'
        ELSE 'NONE'
    END AS AUTH_METHOD,
    CASE 
        WHEN BIN_AND(RDB$LINK_FLAGS, 4) = 4 THEN 'ENABLED'
        ELSE 'DISABLED'
    END AS CONNECTION_POOLING,
    CASE 
        WHEN BIN_AND(RDB$LINK_FLAGS, 8) = 8 THEN 'READ-only'
        ELSE 'read-write'
    END AS ACCESS_MODE,
    CASE 
        WHEN BIN_AND(RDB$LINK_FLAGS, 32) = 32 THEN 'SCHEMA-AWARE'
        ELSE 'LEGACY'
    END AS SCHEMA_MODE,
    RDB$LINK_SCHEMA_NAME AS LOCAL_SCHEMA,
    RDB$LINK_REMOTE_SCHEMA AS REMOTE_SCHEMA,
    CASE RDB$LINK_SCHEMA_MODE
        WHEN 0 THEN 'NO_SCHEMA_AWARENESS'
        WHEN 1 THEN 'FIXED_REMOTE_SCHEMA'
        WHEN 2 THEN 'CONTEXT_AWARE_SCHEMA'
        WHEN 3 THEN 'HIERARCHICAL_MAPPING'
        WHEN 4 THEN 'MIRROR_MODE'
        ELSE 'UNKNOWN'
    END AS SCHEMA_RESOLUTION,
    RDB$LINK_PROVIDER AS PROVIDER,
    RDB$LINK_POOL_MIN AS MIN_CONNECTIONS,
    RDB$LINK_POOL_MAX AS MAX_CONNECTIONS,
    RDB$LINK_TIMEOUT AS SESSION_TIMEOUT,
    RDB$LINK_CREATED AS CREATED_DATE
FROM RDB$DATABASE_LINKS;

GRANT SELECT ON RDB$DATABASE_LINKS_VIEW TO PUBLIC;