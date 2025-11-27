#pragma once

/**
 * Virtual Catalog Infrastructure
 *
 * Phase D: Catalog Cleanup - Virtual catalog support for protocol emulation
 *
 * This module provides the abstraction layer for virtual catalogs that present
 * protocol-specific views of the ScratchBird internal catalog. Virtual catalogs
 * are used to:
 * 1. Implement SQL standard information_schema
 * 2. Support protocol-specific system catalogs (pg_catalog, mysql.*, sys.*)
 * 3. Generate on-demand emulation views for foreign database compatibility
 *
 * Key Design Principle: Emulated system tables are implemented as views that
 * query the internal ScratchBird catalog and transform results to match the
 * emulated format. They are created on-demand when an emulation server/database
 * is configured.
 *
 * Created: November 26, 2025
 * Phase: Catalog Cleanup Phase D
 */

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/typed_value.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace scratchbird::catalog {

using namespace scratchbird::core;

// ============================================================================
// Virtual Catalog Data Structures
// ============================================================================

/**
 * VirtualRow - A row from a virtual catalog query
 *
 * Contains column name-value pairs for flexible schema handling.
 */
struct VirtualRow {
    std::vector<std::pair<std::string, TypedValue>> columns;

    // Helper to get a column value by name
    const TypedValue* getColumn(const std::string& name) const {
        for (const auto& col : columns) {
            if (col.first == name) {
                return &col.second;
            }
        }
        return nullptr;
    }
};

/**
 * VirtualResultSet - Result set from a virtual catalog query
 *
 * Contains column metadata and row data for virtual table queries.
 */
struct VirtualResultSet {
    std::vector<std::string> column_names;    // Column names
    std::vector<DataType> column_types;       // Column types
    std::vector<VirtualRow> rows;             // Result rows

    // Helper to get column index by name
    int getColumnIndex(const std::string& name) const {
        for (size_t i = 0; i < column_names.size(); ++i) {
            if (column_names[i] == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    // Helper to check if result set is empty
    bool empty() const { return rows.empty(); }

    // Helper to get row count
    size_t rowCount() const { return rows.size(); }

    // Helper to get column count
    size_t columnCount() const { return column_names.size(); }
};

/**
 * ProtocolType - Wire protocol for catalog translation
 *
 * Each protocol has its own system catalog format that virtual catalogs
 * must emulate.
 */
enum class ProtocolType : uint8_t {
    SCRATCHBIRD = 0,  // Native ScratchBird (information_schema)
    POSTGRESQL = 1,   // PostgreSQL (pg_catalog.*)
    MYSQL = 2,        // MySQL (mysql.*, information_schema)
    MSSQL = 3,        // SQL Server (sys.*)
    FIREBIRD = 4      // Firebird (RDB$*)
};

/**
 * Convert ProtocolType to string for logging/debugging
 */
inline const char* protocolTypeToString(ProtocolType type) {
    switch (type) {
        case ProtocolType::SCRATCHBIRD: return "scratchbird";
        case ProtocolType::POSTGRESQL:  return "postgresql";
        case ProtocolType::MYSQL:       return "mysql";
        case ProtocolType::MSSQL:       return "mssql";
        case ProtocolType::FIREBIRD:    return "firebird";
        default: return "unknown";
    }
}

// ============================================================================
// Virtual Catalog Handler Interface
// ============================================================================

/**
 * VirtualCatalogHandler - Base class for virtual catalog implementations
 *
 * Handlers implement protocol-specific catalog views. Each handler owns
 * one or more virtual schemas and provides query capabilities for virtual
 * tables within those schemas.
 *
 * Implementations:
 * - InformationSchemaHandler (SQL standard)
 * - PgCatalogHandler (PostgreSQL)
 * - MySQLCatalogHandler (MySQL)
 * - MSSQLCatalogHandler (SQL Server)
 * - FirebirdCatalogHandler (Firebird RDB$*)
 */
class VirtualCatalogHandler {
public:
    virtual ~VirtualCatalogHandler() = default;

    /**
     * Get the protocol type this handler supports
     */
    virtual ProtocolType getProtocolType() const = 0;

    /**
     * Check if this handler owns the given schema
     *
     * @param schema_name Schema name to check (case-insensitive)
     * @return true if this handler provides virtual tables for the schema
     */
    virtual bool ownsSchema(const std::string& schema_name) const = 0;

    /**
     * Check if this handler owns the given table
     *
     * @param schema_name Schema name
     * @param table_name Table name
     * @return true if this handler provides the virtual table
     */
    virtual bool ownsTable(const std::string& schema_name,
                           const std::string& table_name) const = 0;

    /**
     * Execute a query against a virtual table
     *
     * @param schema_name Schema containing the virtual table
     * @param table_name Virtual table name
     * @param where_clause Optional WHERE clause filter (empty = no filter)
     * @param results Output: Query results
     * @param ctx Error context
     * @return Status::OK on success
     */
    virtual Status queryTable(const std::string& schema_name,
                              const std::string& table_name,
                              const std::string& where_clause,
                              VirtualResultSet& results,
                              ErrorContext* ctx = nullptr) = 0;

    /**
     * Get column definitions for a virtual table
     *
     * @param schema_name Schema containing the virtual table
     * @param table_name Virtual table name
     * @param columns Output: Column definitions
     * @param ctx Error context
     * @return Status::OK on success
     */
    virtual Status getTableColumns(const std::string& schema_name,
                                   const std::string& table_name,
                                   std::vector<CatalogManager::ColumnInfo>& columns,
                                   ErrorContext* ctx = nullptr) = 0;

    /**
     * List all virtual tables in a schema
     *
     * @param schema_name Schema name
     * @param table_names Output: List of virtual table names
     * @param ctx Error context
     * @return Status::OK on success
     */
    virtual Status listTables(const std::string& schema_name,
                              std::vector<std::string>& table_names,
                              ErrorContext* ctx = nullptr) = 0;

    /**
     * List all schemas owned by this handler
     *
     * @param schema_names Output: List of schema names
     * @param ctx Error context
     * @return Status::OK on success
     */
    virtual Status listSchemas(std::vector<std::string>& schema_names,
                               ErrorContext* ctx = nullptr) = 0;

protected:
    CatalogManager* catalog_manager_ = nullptr;

    // Helper to set catalog manager reference
    void setCatalogManager(CatalogManager* catalog) {
        catalog_manager_ = catalog;
    }

    friend class VirtualCatalogRouter;
};

// ============================================================================
// Virtual Catalog Router
// ============================================================================

/**
 * VirtualCatalogRouter - Routes queries to appropriate virtual catalog handlers
 *
 * The router is a singleton that manages all virtual catalog handlers and
 * routes queries to the appropriate handler based on schema name and protocol.
 *
 * Usage:
 * 1. Initialize with catalog manager reference
 * 2. Register handlers for each protocol
 * 3. Use isVirtualSchema() to check if a schema is virtual
 * 4. Use routeQuery() to execute queries against virtual tables
 */
class VirtualCatalogRouter {
public:
    /**
     * Get singleton instance
     */
    static VirtualCatalogRouter& getInstance() {
        static VirtualCatalogRouter instance;
        return instance;
    }

    // Prevent copying
    VirtualCatalogRouter(const VirtualCatalogRouter&) = delete;
    VirtualCatalogRouter& operator=(const VirtualCatalogRouter&) = delete;

    /**
     * Initialize with catalog manager reference
     *
     * Must be called before using any other methods.
     *
     * @param catalog CatalogManager instance
     */
    void initialize(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        // Update all registered handlers
        for (auto& pair : handlers_) {
            pair.second->setCatalogManager(catalog);
        }
    }

    /**
     * Register a handler for a protocol
     *
     * @param protocol Protocol type
     * @param handler Handler instance (ownership transferred)
     */
    void registerHandler(ProtocolType protocol,
                         std::unique_ptr<VirtualCatalogHandler> handler) {
        if (catalog_manager_) {
            handler->setCatalogManager(catalog_manager_);
        }
        handlers_[protocol] = std::move(handler);
    }

    /**
     * Get handler for a specific protocol
     *
     * @param protocol Protocol type
     * @return Handler pointer or nullptr if not registered
     */
    VirtualCatalogHandler* getHandler(ProtocolType protocol) {
        auto it = handlers_.find(protocol);
        return (it != handlers_.end()) ? it->second.get() : nullptr;
    }

    /**
     * Check if a schema is virtual (owned by any handler)
     *
     * @param schema_name Schema name to check
     * @return true if any registered handler owns this schema
     */
    bool isVirtualSchema(const std::string& schema_name) const {
        for (const auto& pair : handlers_) {
            if (pair.second->ownsSchema(schema_name)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Find handler that owns a schema
     *
     * @param schema_name Schema name
     * @return Handler pointer or nullptr if not found
     */
    VirtualCatalogHandler* findSchemaHandler(const std::string& schema_name) {
        for (auto& pair : handlers_) {
            if (pair.second->ownsSchema(schema_name)) {
                return pair.second.get();
            }
        }
        return nullptr;
    }

    /**
     * Route a query to the appropriate handler
     *
     * @param protocol Protocol to use for query
     * @param schema_name Schema containing the virtual table
     * @param table_name Virtual table name
     * @param where_clause Optional WHERE clause filter
     * @param results Output: Query results
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status routeQuery(ProtocolType protocol,
                      const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& where_clause,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) {
        // First try protocol-specific handler
        auto* handler = getHandler(protocol);
        if (handler && handler->ownsTable(schema_name, table_name)) {
            return handler->queryTable(schema_name, table_name, where_clause, results, ctx);
        }

        // Fall back to any handler that owns the schema
        handler = findSchemaHandler(schema_name);
        if (handler && handler->ownsTable(schema_name, table_name)) {
            return handler->queryTable(schema_name, table_name, where_clause, results, ctx);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Virtual table not found: " + schema_name + "." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * Get virtual table info (for query planning)
     *
     * @param protocol Protocol to use
     * @param schema_name Schema name
     * @param table_name Table name
     * @param columns Output: Column definitions
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status getVirtualTableColumns(ProtocolType protocol,
                                  const std::string& schema_name,
                                  const std::string& table_name,
                                  std::vector<CatalogManager::ColumnInfo>& columns,
                                  ErrorContext* ctx = nullptr) {
        auto* handler = findSchemaHandler(schema_name);
        if (handler && handler->ownsTable(schema_name, table_name)) {
            return handler->getTableColumns(schema_name, table_name, columns, ctx);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Virtual table not found: " + schema_name + "." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * List all registered handlers
     */
    std::vector<ProtocolType> listRegisteredProtocols() const {
        std::vector<ProtocolType> protocols;
        for (const auto& pair : handlers_) {
            protocols.push_back(pair.first);
        }
        return protocols;
    }

    /**
     * Check if router is initialized
     */
    bool isInitialized() const { return catalog_manager_ != nullptr; }

private:
    VirtualCatalogRouter() = default;

    std::unordered_map<ProtocolType, std::unique_ptr<VirtualCatalogHandler>> handlers_;
    CatalogManager* catalog_manager_ = nullptr;
};

// ============================================================================
// Emulation View Definition
// ============================================================================

/**
 * EmulatedViewDefinition - Template for generating emulated system views
 *
 * Used by EmulationViewGenerator to create protocol-specific system views
 * when an emulated database connection is established.
 */
struct EmulatedViewDefinition {
    std::string view_name;              // e.g., "RDB$RELATIONS", "pg_class"
    std::string source_query;           // SQL that maps to internal catalog
    std::vector<std::string> columns;   // Column names
    std::vector<DataType> column_types; // Column types
    std::string description;            // Documentation
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Case-insensitive string comparison for schema/table names
 */
inline bool equalsCaseInsensitive(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/**
 * Check if a string matches any in a list (case-insensitive)
 */
inline bool matchesAny(const std::string& str,
                       std::initializer_list<const char*> options) {
    for (const char* opt : options) {
        if (equalsCaseInsensitive(str, opt)) {
            return true;
        }
    }
    return false;
}

} // namespace scratchbird::catalog
