#pragma once

#include "scratchbird/core/uuidv7.h"

/**
 * @file system_uuids.h
 * @brief Fixed UUID constants for system objects
 *
 * This file defines well-known, fixed UUIDs for system schemas and system catalog tables.
 * These UUIDs are the same across all ScratchBird databases and must NEVER be changed.
 *
 * UUID Format: UUIDv7 (RFC 9562)
 * - Bytes 0-5: Unix timestamp in milliseconds (0x019200000000 = epoch base for system UUIDs)
 * - Bytes 6-7: Version (0x7000 for UUIDv7)
 * - Bytes 8-9: Variant (0x8000 for RFC 4122)
 * - Bytes 10-15: Sequential system object identifier
 */

namespace scratchbird::core::system_uuids
{

    // =========================================================================
    // SYSTEM SCHEMA UUIDs
    // =========================================================================
    // These are the 8 base system schemas created with every database.
    // They use sequential identifiers 0x01 through 0x08.

    /**
     * [root] - Root/bootstrap schema
     * Contains fundamental database structures
     */
    constexpr UuidV7Bytes ROOT_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00, // Timestamp (base epoch)
        0x70, 0x00,                         // Version 7
        0x80, 0x00,                         // Variant (RFC 4122)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01  // ID: 1
    }};

    /**
     * [sys] - System catalog schema
     * Contains system catalog tables (pg_class equivalent)
     */
    constexpr UuidV7Bytes SYS_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x02  // ID: 2
    }};

    /**
     * [sec] - Security schema
     * Contains authentication, authorization, and audit tables
     */
    constexpr UuidV7Bytes SEC_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x03  // ID: 3
    }};

    /**
     * [agents] - Agent configuration schema
     * For autonomous agent system (future feature)
     */
    constexpr UuidV7Bytes AGENTS_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x04  // ID: 4
    }};

    /**
     * [app] - Application data schema
     * Default schema for user applications
     */
    constexpr UuidV7Bytes APP_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x05  // ID: 5
    }};

    /**
     * [remote] - Remote database links schema
     * For distributed database features (future)
     */
    constexpr UuidV7Bytes REMOTE_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x06  // ID: 6
    }};

    /**
     * [users] - User management schema
     * Contains user accounts and profiles
     */
    constexpr UuidV7Bytes USERS_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x07  // ID: 7
    }};

    /**
     * [roles] - Role-based access control schema
     * Contains role definitions and permissions
     */
    constexpr UuidV7Bytes ROLES_SCHEMA_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x08  // ID: 8
    }};

    // Array for easy iteration during database creation
    constexpr UuidV7Bytes SYSTEM_SCHEMA_UUIDS[] = {
        ROOT_SCHEMA_UUID,
        SYS_SCHEMA_UUID,
        SEC_SCHEMA_UUID,
        AGENTS_SCHEMA_UUID,
        APP_SCHEMA_UUID,
        REMOTE_SCHEMA_UUID,
        USERS_SCHEMA_UUID,
        ROLES_SCHEMA_UUID
    };

    // =========================================================================
    // SYSTEM CATALOG TABLE UUIDs
    // =========================================================================
    // These are the system catalog tables that store metadata.
    // They use sequential identifiers starting at 0x10.

    /**
     * System catalog table: schemas
     * Stores schema definitions (equivalent to pg_namespace)
     */
    constexpr UuidV7Bytes SCHEMAS_TABLE_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x10  // ID: 16 (0x10)
    }};

    /**
     * System catalog table: tables
     * Stores table definitions (equivalent to pg_class for tables)
     */
    constexpr UuidV7Bytes TABLES_TABLE_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x11  // ID: 17 (0x11)
    }};

    /**
     * System catalog table: columns
     * Stores column definitions (equivalent to pg_attribute)
     */
    constexpr UuidV7Bytes COLUMNS_TABLE_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x12  // ID: 18 (0x12)
    }};

    /**
     * System catalog table: indexes
     * Stores index definitions (equivalent to pg_index)
     */
    constexpr UuidV7Bytes INDEXES_TABLE_UUID = {{
        0x01, 0x92, 0x00, 0x00, 0x00, 0x00,
        0x70, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x13  // ID: 19 (0x13)
    }};

    // =========================================================================
    // UTILITY FUNCTIONS
    // =========================================================================

    /**
     * Check if a UUID is a system schema UUID
     */
    inline auto isSystemSchema(const UuidV7Bytes &uuid) -> bool
    {
        for (const auto &sys_uuid : SYSTEM_SCHEMA_UUIDS)
        {
            if (uuid == sys_uuid)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * Check if a UUID is a system catalog table UUID
     */
    inline auto isSystemCatalogTable(const UuidV7Bytes &uuid) -> bool
    {
        return uuid == SCHEMAS_TABLE_UUID ||
               uuid == TABLES_TABLE_UUID ||
               uuid == COLUMNS_TABLE_UUID ||
               uuid == INDEXES_TABLE_UUID;
    }

    /**
     * Get the name of a system schema by its UUID
     * Returns nullptr if not a system schema
     */
    inline auto getSystemSchemaName(const UuidV7Bytes &uuid) -> const char *
    {
        if (uuid == ROOT_SCHEMA_UUID) return "[root]";
        if (uuid == SYS_SCHEMA_UUID) return "[sys]";
        if (uuid == SEC_SCHEMA_UUID) return "[sec]";
        if (uuid == AGENTS_SCHEMA_UUID) return "[agents]";
        if (uuid == APP_SCHEMA_UUID) return "[app]";
        if (uuid == REMOTE_SCHEMA_UUID) return "[remote]";
        if (uuid == USERS_SCHEMA_UUID) return "[users]";
        if (uuid == ROLES_SCHEMA_UUID) return "[roles]";
        return nullptr;
    }

} // namespace scratchbird::core::system_uuids
