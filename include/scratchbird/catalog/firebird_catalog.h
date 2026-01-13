#pragma once

/**
 * Firebird RDB$ Catalog Emulation Handler
 *
 * Phase D: Catalog Cleanup - Firebird system catalog emulation
 *
 * This module implements Firebird-compatible RDB$, MON$, and SEC$ views that
 * allow Firebird client tools (FlameRobin, IBExpert, etc.) to introspect the
 * ScratchBird catalog as if it were a Firebird database.
 *
 * Implemented Views (Firebird 5.0 compatible):
 *
 * RDB$ (Core System Tables):
 * - RDB$DATABASE: Database metadata
 * - RDB$RELATIONS: Tables and views
 * - RDB$FIELDS: Domain/column definitions
 * - RDB$RELATION_FIELDS: Table columns
 * - RDB$INDICES: Indexes
 * - RDB$INDEX_SEGMENTS: Index columns
 * - RDB$GENERATORS: Sequences (generators)
 * - RDB$PROCEDURES: Stored procedures
 * - RDB$PROCEDURE_PARAMETERS: Procedure parameters
 * - RDB$FUNCTIONS: User-defined functions
 * - RDB$FUNCTION_ARGUMENTS: Function parameters
 * - RDB$TRIGGERS: Triggers
 * - RDB$EXCEPTIONS: User exceptions
 * - RDB$CONSTRAINTS: Primary/foreign key constraints
 * - RDB$CHECK_CONSTRAINTS: Check constraints
 * - RDB$REF_CONSTRAINTS: Foreign key references
 * - RDB$USER_PRIVILEGES: Permissions
 * - RDB$ROLES: Role definitions
 * - RDB$CHARACTER_SETS: Character sets
 * - RDB$COLLATIONS: Collations
 * - RDB$TYPES: Type enumerations
 * - RDB$DEPENDENCIES: Object dependencies
 *
 * MON$ (Monitoring Tables):
 * - MON$DATABASE: Database statistics
 * - MON$ATTACHMENTS: Active connections
 * - MON$TRANSACTIONS: Active transactions
 * - MON$STATEMENTS: Executing statements
 *
 * SEC$ (Security Tables):
 * - SEC$USERS: Database users
 * - SEC$USER_ATTRIBUTES: User extended attributes
 *
 * Created: December 7, 2025
 * Phase: Catalog Cleanup Phase D / Firebird Emulation
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/typed_value.h"
#include <string>
#include <vector>
#include <unordered_set>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * FirebirdCatalogHandler - Firebird RDB$/MON$/SEC$ catalog emulation
 *
 * Provides virtual views that query the ScratchBird internal catalog and
 * present data in Firebird system table format for tool compatibility.
 *
 * Note: Firebird uses CHAR(63) UNICODE_FSS for most identifier columns.
 * We map ScratchBird names to this format for compatibility.
 *
 * Usage:
 *   SELECT * FROM RDB$RELATIONS WHERE RDB$RELATION_NAME = 'USERS';
 *   SELECT * FROM RDB$RELATION_FIELDS WHERE RDB$RELATION_NAME = 'USERS';
 *   SELECT * FROM MON$DATABASE;
 */
class FirebirdCatalogHandler : public VirtualCatalogHandler {
public:
    /**
     * Constructor - takes catalog manager reference
     */
    explicit FirebirdCatalogHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    /**
     * Get the protocol type
     */
    ProtocolType getProtocolType() const override {
        return ProtocolType::FIREBIRD;
    }

    /**
     * Check if this handler owns the given schema
     * @return true for Firebird system schemas
     *
     * Firebird system tables don't live in a separate schema - they're
     * in the default schema with RDB$, MON$, SEC$ prefixes. We handle
     * them as if they're in a virtual "" schema or match any schema.
     */
    bool ownsSchema(const std::string& schema_name) const override {
        // Firebird system tables are accessed without schema prefix
        // so we match empty schema or common names like "sys" or "rdb"
        return schema_name.empty() ||
               equalsCaseInsensitive(schema_name, "rdb") ||
               equalsCaseInsensitive(schema_name, "rdb$") ||
               equalsCaseInsensitive(schema_name, "mon$") ||
               equalsCaseInsensitive(schema_name, "sec$") ||
               equalsCaseInsensitive(schema_name, "firebird") ||
               equalsCaseInsensitive(schema_name, "system");
    }

    /**
     * Check if this handler owns the given table
     */
    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override;

    /**
     * Execute a query against a virtual table
     */
    Status queryTable(const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& where_clause,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) override;

    /**
     * Get column definitions for a virtual table
     */
    Status getTableColumns(const std::string& schema_name,
                           const std::string& table_name,
                           std::vector<CatalogManager::ColumnInfo>& columns,
                           ErrorContext* ctx = nullptr) override;

    /**
     * List all virtual tables in a schema
     */
    Status listTables(const std::string& schema_name,
                      std::vector<std::string>& table_names,
                      ErrorContext* ctx = nullptr) override;

    /**
     * List all schemas owned by this handler
     */
    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* ctx = nullptr) override;

private:
    // Table names for ownership checking
    std::unordered_set<std::string> rdb_table_names_;
    std::unordered_set<std::string> mon_table_names_;
    std::unordered_set<std::string> sec_table_names_;

    // Initialize table name sets
    void initializeTableNames();

    // Query implementations for each table
    Status queryRdbDatabase(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbRelations(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbFields(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbRelationFields(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbIndices(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbIndexSegments(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbGenerators(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbProcedures(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbProcedureParameters(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbFunctions(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbFunctionArguments(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbTriggers(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbConstraints(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbRelationConstraints(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbViewRelations(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbCheckConstraints(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbRefConstraints(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbExceptions(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbCharacterSets(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbCollations(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbTypes(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbUserPrivileges(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbRoles(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbDependencies(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbPackages(VirtualResultSet& results, ErrorContext* ctx);
    Status queryRdbKeywords(VirtualResultSet& results, ErrorContext* ctx);

    Status queryMonDatabase(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonAttachments(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonTransactions(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonStatements(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonCallStack(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonIoStats(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonRecordStats(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonMemoryUsage(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonTableStats(VirtualResultSet& results, ErrorContext* ctx);
    Status queryMonContextVariables(VirtualResultSet& results, ErrorContext* ctx);

    Status querySecUsers(VirtualResultSet& results, ErrorContext* ctx);
    Status querySecUserAttributes(VirtualResultSet& results, ErrorContext* ctx);
    Status querySecDbCreators(VirtualResultSet& results, ErrorContext* ctx);
    Status querySecGlobalAuthMapping(VirtualResultSet& results, ErrorContext* ctx);

    // Helper to get columns for a specific table
    void getRdbDatabaseColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbRelationsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbFieldsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbRelationFieldsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbIndicesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbIndexSegmentsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbGeneratorsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbProceduresColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbProcedureParametersColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbFunctionsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbFunctionArgumentsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbTriggersColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbRelationConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbViewRelationsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbCheckConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbRefConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbExceptionsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbCharacterSetsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbCollationsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbTypesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbUserPrivilegesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbRolesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbDependenciesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbPackagesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getRdbKeywordsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonDatabaseColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonAttachmentsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonTransactionsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonStatementsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonCallStackColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonIoStatsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonRecordStatsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonMemoryUsageColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonTableStatsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getMonContextVariablesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getSecUsersColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getSecUserAttributesColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getSecDbCreatorsColumns(std::vector<CatalogManager::ColumnInfo>& cols);
    void getSecGlobalAuthMappingColumns(std::vector<CatalogManager::ColumnInfo>& cols);

    // Helper to uppercase a name (Firebird convention)
    static std::string toUpperCase(const std::string& s);

    // Helper to pad a string to Firebird CHAR(63) format
    static std::string padToChar63(const std::string& s);
};

} // namespace scratchbird::catalog
