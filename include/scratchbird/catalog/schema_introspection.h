/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * Schema Introspection
 * 
 * Provides information_schema and pg_catalog views for PostgreSQL compatibility,
 * and equivalent views for MySQL and Firebird.
 */

#include "scratchbird/core/database.h"
#include <vector>
#include <string>

namespace scratchbird {
namespace catalog {

/**
 * Schema introspection utility
 * 
 * Creates and manages metadata views for database introspection.
 * Compatible with PostgreSQL, MySQL, and Firebird clients.
 */
class SchemaIntrospection {
public:
    // ========================================================================
    // Structs for introspection results
    // ========================================================================
    
    struct TableInfo {
        std::string catalog;
        std::string schema;
        std::string name;
        std::string type;  // BASE TABLE, VIEW, etc.
        std::string owner;
        std::string description;
    };
    
    struct ColumnInfo {
        std::string catalog;
        std::string schema;
        std::string table;
        std::string name;
        int position;
        std::string data_type;
        int max_length;
        int numeric_precision;
        int numeric_scale;
        bool is_nullable;
        std::string default_value;
        bool is_identity;
        std::string collation;
    };
    
    struct IndexInfo {
        std::string catalog;
        std::string schema;
        std::string table;
        std::string name;
        bool is_unique;
        bool is_primary;
        std::vector<std::string> columns;
        std::string type;
    };
    
    struct ConstraintInfo {
        std::string catalog;
        std::string schema;
        std::string table;
        std::string name;
        std::string type;  // PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK
        std::vector<std::string> columns;
        std::string referenced_table;
        std::vector<std::string> referenced_columns;
        std::string check_clause;
    };
    
    // ========================================================================
    // PostgreSQL pg_catalog Views
    // ========================================================================
    
    static const char* PG_CATALOG_TABLES;
    static const char* PG_CATALOG_COLUMNS;
    static const char* PG_CATALOG_INDEXES;
    static const char* PG_CATALOG_DATABASES;
    static const char* PG_CATALOG_SETTINGS;
    static const char* PG_CATALOG_VIEWS;
    static const char* PG_CATALOG_TYPES;
    static const char* PG_CATALOG_NAMESPACE;
    
    // ========================================================================
    // information_schema Views
    // ========================================================================
    
    static const char* INFO_SCHEMA_TABLES;
    static const char* INFO_SCHEMA_COLUMNS;
    static const char* INFO_SCHEMA_VIEWS;
    static const char* INFO_SCHEMA_SCHEMATA;
    static const char* INFO_SCHEMA_KEY_COLUMN_USAGE;
    static const char* INFO_SCHEMA_TABLE_CONSTRAINTS;
    
    // ========================================================================
    // MySQL Compatibility Views
    // ========================================================================
    
    static const char* MYSQL_SHOW_DATABASES;
    static const char* MYSQL_SHOW_TABLES;
    static const char* MYSQL_SHOW_COLUMNS;
    static const char* MYSQL_SHOW_INDEX;
    static const char* MYSQL_SHOW_CREATE_TABLE;
    
    // ========================================================================
    // Firebird RDB$ Views
    // ========================================================================
    
    static const char* FB_RDB_RELATIONS;
    static const char* FB_RDB_RELATION_FIELDS;
    static const char* FB_RDB_FIELDS;
    static const char* FB_RDB_INDICES;
    
    // ========================================================================
    // Initialization
    // ========================================================================
    
    /**
     * Initialize pg_catalog and information_schema for PostgreSQL compatibility
     */
    static void initializePostgreSQL(core::Database* db);
    
    /**
     * Initialize mysql schema for MySQL compatibility
     */
    static void initializeMySQL(core::Database* db);
    
    /**
     * Initialize RDB$ system tables for Firebird compatibility
     */
    static void initializeFirebird(core::Database* db);
    
    // ========================================================================
    // Query Methods
    // ========================================================================
    
    /**
     * Get list of tables
     */
    static std::vector<TableInfo> getTables(core::Database* db,
                                           const std::string& schema);
    
    /**
     * Get columns for a table
     */
    static std::vector<ColumnInfo> getColumns(core::Database* db,
                                             const std::string& schema,
                                             const std::string& table);
    
    /**
     * Get indexes for a table
     */
    static std::vector<IndexInfo> getIndexes(core::Database* db,
                                            const std::string& schema,
                                            const std::string& table);
    
    /**
     * Get constraints for a table
     */
    static std::vector<ConstraintInfo> getConstraints(core::Database* db,
                                                     const std::string& schema,
                                                     const std::string& table);
    
    /**
     * Get primary key columns for a table
     */
    static std::vector<std::string> getPrimaryKeyColumns(core::Database* db,
                                                        const std::string& schema,
                                                        const std::string& table);
    
    /**
     * Get foreign keys for a table
     */
    static std::vector<ConstraintInfo> getForeignKeys(core::Database* db,
                                                     const std::string& schema,
                                                     const std::string& table);
};

} // namespace catalog
} // namespace scratchbird
