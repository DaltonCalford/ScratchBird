/**
 * Virtual Catalog Implementation
 *
 * Phase D: Catalog Cleanup - Virtual catalog router and initialization
 *
 * This module provides the implementation for the virtual catalog routing
 * system and handler registration.
 *
 * Created: November 26, 2025
 * Phase: Catalog Cleanup Phase D
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/catalog/information_schema.h"
#include "scratchbird/catalog/sys_catalog.h"
#include "scratchbird/catalog/pg_catalog.h"
#include "scratchbird/catalog/mysql_catalog.h"
#include "scratchbird/catalog/firebird_catalog.h"
// TEMPORARILY DISABLED: mssql_catalog.h has pre-existing API mismatch issues
// #include "scratchbird/catalog/mssql_catalog.h"

namespace scratchbird::catalog {

// ============================================================================
// Virtual Catalog Initialization
// ============================================================================

/**
 * Initialize all built-in virtual catalog handlers
 *
 * Call this function during server/database startup to register all
 * built-in virtual catalog handlers (information_schema, pg_catalog,
 * mysql.*, sys.*).
 *
 * @param catalog CatalogManager instance
 */
void initializeVirtualCatalogs(CatalogManager* catalog) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();
    router.initialize(catalog);

    // Register information_schema (SQL standard - all protocols)
    router.registerHandler(ProtocolType::SCRATCHBIRD,
        std::make_unique<InformationSchemaHandler>(catalog));
    // Register sys.* (ScratchBird native system catalog)
    router.registerHandler(ProtocolType::SCRATCHBIRD,
        std::make_unique<SysCatalogHandler>(catalog));

    // Register pg_catalog (PostgreSQL wire protocol)
    router.registerHandler(ProtocolType::POSTGRESQL,
        std::make_unique<PgCatalogHandler>(catalog));

    // Register mysql.* (MySQL wire protocol)
    router.registerHandler(ProtocolType::MYSQL,
        std::make_unique<MySQLCatalogHandler>(catalog));

    // Register RDB$, MON$, SEC$ (Firebird wire protocol)
    router.registerHandler(ProtocolType::FIREBIRD,
        std::make_unique<FirebirdCatalogHandler>(catalog));

    // TEMPORARILY DISABLED: MSSQLCatalogHandler has pre-existing API mismatch issues
    // Register sys.* (SQL Server wire protocol / TDS)
    // router.registerHandler(ProtocolType::MSSQL,
    //     std::make_unique<MSSQLCatalogHandler>(catalog));
}

/**
 * Shutdown virtual catalog system
 *
 * Call during server shutdown to cleanup handlers.
 */
void shutdownVirtualCatalogs() {
    // Router is a singleton, handlers will be cleaned up automatically
    // when the static instance is destroyed
}

// ============================================================================
// Virtual Catalog Query Helper Functions
// ============================================================================

/**
 * Execute a virtual catalog query and convert to standard format
 *
 * This is a convenience function for executing virtual catalog queries
 * from the query execution layer.
 *
 * @param protocol Protocol type for the query
 * @param schema_name Schema name (e.g., "information_schema", "pg_catalog")
 * @param table_name Table name (e.g., "tables", "pg_class")
 * @param where_clause Optional WHERE clause filter
 * @param results Output result set
 * @param ctx Error context
 * @return Status::OK on success
 */
Status executeVirtualQuery(ProtocolType protocol,
                           const std::string& schema_name,
                           const std::string& table_name,
                           const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();

    if (!router.isInitialized()) {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                          "Virtual catalog system not initialized");
        return Status::INTERNAL_ERROR;
    }

    return router.routeQuery(protocol, schema_name, table_name, where_clause,
                             results, ctx);
}

/**
 * Check if a schema.table reference is a virtual table
 *
 * @param schema_name Schema name
 * @param table_name Table name (optional - if empty, just checks schema)
 * @return true if the schema or table is virtual
 */
bool isVirtualTable(const std::string& schema_name,
                    const std::string& table_name) {
    VirtualCatalogRouter& router = VirtualCatalogRouter::getInstance();

    if (!router.isInitialized()) {
        return false;
    }

    if (!router.isVirtualSchema(schema_name)) {
        return false;
    }

    if (table_name.empty()) {
        return true;  // Schema itself is virtual
    }

    // Check if specific table exists
    VirtualCatalogHandler* handler = router.findSchemaHandler(schema_name);
    if (!handler) {
        return false;
    }

    return handler->ownsTable(schema_name, table_name);
}

/**
 * Get protocol type from string name
 *
 * @param name Protocol name (e.g., "postgresql", "mysql")
 * @return ProtocolType enum value
 */
ProtocolType protocolTypeFromString(const std::string& name) {
    if (equalsCaseInsensitive(name, "postgresql") ||
        equalsCaseInsensitive(name, "postgres") ||
        equalsCaseInsensitive(name, "pg")) {
        return ProtocolType::POSTGRESQL;
    }
    if (equalsCaseInsensitive(name, "mysql")) {
        return ProtocolType::MYSQL;
    }
    if (equalsCaseInsensitive(name, "mssql") ||
        equalsCaseInsensitive(name, "sqlserver") ||
        equalsCaseInsensitive(name, "tds")) {
        return ProtocolType::MSSQL;
    }
    if (equalsCaseInsensitive(name, "firebird") ||
        equalsCaseInsensitive(name, "fb")) {
        return ProtocolType::FIREBIRD;
    }
    return ProtocolType::SCRATCHBIRD;
}

// ============================================================================
// VirtualResultSet Conversion Utilities
// ============================================================================

/**
 * Convert VirtualResultSet column value to string for display
 *
 * @param value TypedValue to convert
 * @return String representation
 */
std::string virtualValueToString(const TypedValue& value) {
    if (value.isNull()) {
        return "NULL";
    }

    switch (value.type()) {
        case DataType::BOOLEAN:
            return value.getBoolean() ? "true" : "false";
        case DataType::INT16:
            return std::to_string(value.getUInt16());
        case DataType::INT32:
            return std::to_string(value.getInt32());
        case DataType::INT64:
            return std::to_string(value.getInt64());
        case DataType::FLOAT32:
            return std::to_string(value.getFloat32());
        case DataType::FLOAT64:
            return std::to_string(value.getFloat64());
        case DataType::VARCHAR:
            return value.getVarchar();
        case DataType::CHAR:
            return value.getChar();
        case DataType::TEXT:
            return value.getText();
        default:
            return value.toString();
    }
}

/**
 * Print VirtualResultSet to output stream (for debugging)
 *
 * @param results Result set to print
 * @param out Output stream
 */
void printVirtualResultSet(const VirtualResultSet& results, std::ostream& out) {
    // Print column headers
    for (size_t i = 0; i < results.column_names.size(); ++i) {
        if (i > 0) out << "\t";
        out << results.column_names[i];
    }
    out << "\n";

    // Print separator
    for (size_t i = 0; i < results.column_names.size(); ++i) {
        if (i > 0) out << "\t";
        out << std::string(results.column_names[i].length(), '-');
    }
    out << "\n";

    // Print rows
    for (const auto& row : results.rows) {
        for (size_t i = 0; i < row.columns.size(); ++i) {
            if (i > 0) out << "\t";
            out << virtualValueToString(row.columns[i].second);
        }
        out << "\n";
    }

    out << "\n(" << results.rows.size() << " rows)\n";
}

} // namespace scratchbird::catalog
