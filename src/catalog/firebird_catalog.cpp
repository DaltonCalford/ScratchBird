/**
 * Firebird RDB$ Catalog Emulation Handler Implementation
 *
 * Maps ScratchBird internal catalog to Firebird RDB$, MON$, SEC$ system tables.
 */

#include "scratchbird/catalog/firebird_catalog.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/proc_array.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <unordered_map>
#include <unistd.h>

namespace scratchbird::catalog {

// ============================================================================
// Initialization
// ============================================================================

void FirebirdCatalogHandler::initializeTableNames() {
    // RDB$ tables (core system tables)
    rdb_table_names_ = {
        "RDB$DATABASE",
        "RDB$RELATIONS",
        "RDB$FIELDS",
        "RDB$RELATION_FIELDS",
        "RDB$INDICES",
        "RDB$INDEX_SEGMENTS",
        "RDB$GENERATORS",
        "RDB$PROCEDURES",
        "RDB$PROCEDURE_PARAMETERS",
        "RDB$FUNCTIONS",
        "RDB$FUNCTION_ARGUMENTS",
        "RDB$TRIGGERS",
        "RDB$EXCEPTIONS",
        "RDB$CONSTRAINTS",
        "RDB$RELATION_CONSTRAINTS",
        "RDB$VIEW_RELATIONS",
        "RDB$CHECK_CONSTRAINTS",
        "RDB$REF_CONSTRAINTS",
        "RDB$USER_PRIVILEGES",
        "RDB$ROLES",
        "RDB$CHARACTER_SETS",
        "RDB$COLLATIONS",
        "RDB$TYPES",
        "RDB$DEPENDENCIES",
        "RDB$PACKAGES",
        "RDB$KEYWORDS"
    };

    // MON$ tables (monitoring)
    mon_table_names_ = {
        "MON$DATABASE",
        "MON$ATTACHMENTS",
        "MON$TRANSACTIONS",
        "MON$STATEMENTS",
        "MON$CALL_STACK",
        "MON$IO_STATS",
        "MON$RECORD_STATS",
        "MON$MEMORY_USAGE",
        "MON$TABLE_STATS",
        "MON$CONTEXT_VARIABLES"
    };

    // SEC$ tables (security)
    sec_table_names_ = {
        "SEC$USERS",
        "SEC$USER_ATTRIBUTES",
        "SEC$DB_CREATORS",
        "SEC$GLOBAL_AUTH_MAPPING"
    };
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string FirebirdCatalogHandler::toUpperCase(const std::string& s) {
    std::string result = s;
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

std::string FirebirdCatalogHandler::padToChar63(const std::string& s) {
    // Firebird uses CHAR(63) for identifiers - we just return as-is
    // since ScratchBird handles variable-length properly
    return s;
}

// ============================================================================
// Ownership Checking
// ============================================================================

bool FirebirdCatalogHandler::ownsTable(const std::string& /* schema_name */,
                                        const std::string& table_name) const {
    std::string upper = toUpperCase(table_name);

    if (rdb_table_names_.count(upper) > 0) return true;
    if (mon_table_names_.count(upper) > 0) return true;
    if (sec_table_names_.count(upper) > 0) return true;

    return false;
}

// ============================================================================
// Schema/Table Listing
// ============================================================================

Status FirebirdCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                           ErrorContext* /* ctx */) {
    // Firebird system tables are typically accessed without schema
    schema_names = {"", "RDB$", "MON$", "SEC$"};
    return Status::OK;
}

Status FirebirdCatalogHandler::listTables(const std::string& schema_name,
                                          std::vector<std::string>& table_names,
                                          ErrorContext* /* ctx */) {
    table_names.clear();

    std::string upper = toUpperCase(schema_name);

    if (upper.empty() || upper == "SYSTEM" || upper == "FIREBIRD") {
        // List all
        for (const auto& t : rdb_table_names_) table_names.push_back(t);
        for (const auto& t : mon_table_names_) table_names.push_back(t);
        for (const auto& t : sec_table_names_) table_names.push_back(t);
    } else if (upper == "RDB$" || upper == "RDB") {
        for (const auto& t : rdb_table_names_) table_names.push_back(t);
    } else if (upper == "MON$" || upper == "MON") {
        for (const auto& t : mon_table_names_) table_names.push_back(t);
    } else if (upper == "SEC$" || upper == "SEC") {
        for (const auto& t : sec_table_names_) table_names.push_back(t);
    }

    return Status::OK;
}

// ============================================================================
// Query Routing
// ============================================================================

Status FirebirdCatalogHandler::queryTable(const std::string& /* schema_name */,
                                          const std::string& table_name,
                                          const std::string& /* where_clause */,
                                          VirtualResultSet& results,
                                          ErrorContext* ctx) {
    std::string upper = toUpperCase(table_name);

    // RDB$ tables
    if (upper == "RDB$DATABASE") return queryRdbDatabase(results, ctx);
    if (upper == "RDB$RELATIONS") return queryRdbRelations(results, ctx);
    if (upper == "RDB$FIELDS") return queryRdbFields(results, ctx);
    if (upper == "RDB$RELATION_FIELDS") return queryRdbRelationFields(results, ctx);
    if (upper == "RDB$INDICES") return queryRdbIndices(results, ctx);
    if (upper == "RDB$INDEX_SEGMENTS") return queryRdbIndexSegments(results, ctx);
    if (upper == "RDB$GENERATORS") return queryRdbGenerators(results, ctx);
    if (upper == "RDB$PROCEDURES") return queryRdbProcedures(results, ctx);
    if (upper == "RDB$PROCEDURE_PARAMETERS") return queryRdbProcedureParameters(results, ctx);
    if (upper == "RDB$FUNCTIONS") return queryRdbFunctions(results, ctx);
    if (upper == "RDB$FUNCTION_ARGUMENTS") return queryRdbFunctionArguments(results, ctx);
    if (upper == "RDB$TRIGGERS") return queryRdbTriggers(results, ctx);
    if (upper == "RDB$CONSTRAINTS") return queryRdbConstraints(results, ctx);
    if (upper == "RDB$RELATION_CONSTRAINTS") return queryRdbRelationConstraints(results, ctx);
    if (upper == "RDB$VIEW_RELATIONS") return queryRdbViewRelations(results, ctx);
    if (upper == "RDB$CHECK_CONSTRAINTS") return queryRdbCheckConstraints(results, ctx);
    if (upper == "RDB$REF_CONSTRAINTS") return queryRdbRefConstraints(results, ctx);
    if (upper == "RDB$EXCEPTIONS") return queryRdbExceptions(results, ctx);
    if (upper == "RDB$CHARACTER_SETS") return queryRdbCharacterSets(results, ctx);
    if (upper == "RDB$COLLATIONS") return queryRdbCollations(results, ctx);
    if (upper == "RDB$TYPES") return queryRdbTypes(results, ctx);
    if (upper == "RDB$USER_PRIVILEGES") return queryRdbUserPrivileges(results, ctx);
    if (upper == "RDB$ROLES") return queryRdbRoles(results, ctx);
    if (upper == "RDB$DEPENDENCIES") return queryRdbDependencies(results, ctx);
    if (upper == "RDB$PACKAGES") return queryRdbPackages(results, ctx);
    if (upper == "RDB$KEYWORDS") return queryRdbKeywords(results, ctx);

    // MON$ tables
    if (upper == "MON$DATABASE") return queryMonDatabase(results, ctx);
    if (upper == "MON$ATTACHMENTS") return queryMonAttachments(results, ctx);
    if (upper == "MON$TRANSACTIONS") return queryMonTransactions(results, ctx);
    if (upper == "MON$STATEMENTS") return queryMonStatements(results, ctx);
    if (upper == "MON$CALL_STACK") return queryMonCallStack(results, ctx);
    if (upper == "MON$IO_STATS") return queryMonIoStats(results, ctx);
    if (upper == "MON$RECORD_STATS") return queryMonRecordStats(results, ctx);
    if (upper == "MON$MEMORY_USAGE") return queryMonMemoryUsage(results, ctx);
    if (upper == "MON$TABLE_STATS") return queryMonTableStats(results, ctx);
    if (upper == "MON$CONTEXT_VARIABLES") return queryMonContextVariables(results, ctx);

    // SEC$ tables
    if (upper == "SEC$USERS") return querySecUsers(results, ctx);
    if (upper == "SEC$USER_ATTRIBUTES") return querySecUserAttributes(results, ctx);
    if (upper == "SEC$DB_CREATORS") return querySecDbCreators(results, ctx);
    if (upper == "SEC$GLOBAL_AUTH_MAPPING") return querySecGlobalAuthMapping(results, ctx);

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Unknown Firebird system table: " + table_name).c_str());
    return Status::NOT_FOUND;
}

// ============================================================================
// Column Definitions
// ============================================================================

Status FirebirdCatalogHandler::getTableColumns(const std::string& /* schema_name */,
                                               const std::string& table_name,
                                               std::vector<CatalogManager::ColumnInfo>& columns,
                                               ErrorContext* ctx) {
    std::string upper = toUpperCase(table_name);

    if (upper == "RDB$DATABASE") { getRdbDatabaseColumns(columns); return Status::OK; }
    if (upper == "RDB$RELATIONS") { getRdbRelationsColumns(columns); return Status::OK; }
    if (upper == "RDB$FIELDS") { getRdbFieldsColumns(columns); return Status::OK; }
    if (upper == "RDB$RELATION_FIELDS") { getRdbRelationFieldsColumns(columns); return Status::OK; }
    if (upper == "RDB$INDICES") { getRdbIndicesColumns(columns); return Status::OK; }
    if (upper == "RDB$INDEX_SEGMENTS") { getRdbIndexSegmentsColumns(columns); return Status::OK; }
    if (upper == "RDB$GENERATORS") { getRdbGeneratorsColumns(columns); return Status::OK; }
    if (upper == "RDB$PROCEDURES") { getRdbProceduresColumns(columns); return Status::OK; }
    if (upper == "RDB$PROCEDURE_PARAMETERS") { getRdbProcedureParametersColumns(columns); return Status::OK; }
    if (upper == "RDB$FUNCTIONS") { getRdbFunctionsColumns(columns); return Status::OK; }
    if (upper == "RDB$FUNCTION_ARGUMENTS") { getRdbFunctionArgumentsColumns(columns); return Status::OK; }
    if (upper == "RDB$TRIGGERS") { getRdbTriggersColumns(columns); return Status::OK; }
    if (upper == "RDB$CONSTRAINTS") { getRdbConstraintsColumns(columns); return Status::OK; }
    if (upper == "RDB$RELATION_CONSTRAINTS") { getRdbRelationConstraintsColumns(columns); return Status::OK; }
    if (upper == "RDB$VIEW_RELATIONS") { getRdbViewRelationsColumns(columns); return Status::OK; }
    if (upper == "RDB$CHECK_CONSTRAINTS") { getRdbCheckConstraintsColumns(columns); return Status::OK; }
    if (upper == "RDB$REF_CONSTRAINTS") { getRdbRefConstraintsColumns(columns); return Status::OK; }
    if (upper == "RDB$EXCEPTIONS") { getRdbExceptionsColumns(columns); return Status::OK; }
    if (upper == "MON$DATABASE") { getMonDatabaseColumns(columns); return Status::OK; }
    if (upper == "MON$ATTACHMENTS") { getMonAttachmentsColumns(columns); return Status::OK; }
    if (upper == "MON$TRANSACTIONS") { getMonTransactionsColumns(columns); return Status::OK; }
    if (upper == "MON$STATEMENTS") { getMonStatementsColumns(columns); return Status::OK; }
    if (upper == "MON$CALL_STACK") { getMonCallStackColumns(columns); return Status::OK; }
    if (upper == "MON$IO_STATS") { getMonIoStatsColumns(columns); return Status::OK; }
    if (upper == "MON$RECORD_STATS") { getMonRecordStatsColumns(columns); return Status::OK; }
    if (upper == "MON$MEMORY_USAGE") { getMonMemoryUsageColumns(columns); return Status::OK; }
    if (upper == "MON$TABLE_STATS") { getMonTableStatsColumns(columns); return Status::OK; }
    if (upper == "MON$CONTEXT_VARIABLES") { getMonContextVariablesColumns(columns); return Status::OK; }
    if (upper == "SEC$USERS") { getSecUsersColumns(columns); return Status::OK; }
    if (upper == "SEC$USER_ATTRIBUTES") { getSecUserAttributesColumns(columns); return Status::OK; }
    if (upper == "SEC$DB_CREATORS") { getSecDbCreatorsColumns(columns); return Status::OK; }
    if (upper == "SEC$GLOBAL_AUTH_MAPPING") { getSecGlobalAuthMappingColumns(columns); return Status::OK; }
    if (upper == "RDB$CHARACTER_SETS") { getRdbCharacterSetsColumns(columns); return Status::OK; }
    if (upper == "RDB$COLLATIONS") { getRdbCollationsColumns(columns); return Status::OK; }
    if (upper == "RDB$TYPES") { getRdbTypesColumns(columns); return Status::OK; }
    if (upper == "RDB$USER_PRIVILEGES") { getRdbUserPrivilegesColumns(columns); return Status::OK; }
    if (upper == "RDB$ROLES") { getRdbRolesColumns(columns); return Status::OK; }
    if (upper == "RDB$DEPENDENCIES") { getRdbDependenciesColumns(columns); return Status::OK; }
    if (upper == "RDB$PACKAGES") { getRdbPackagesColumns(columns); return Status::OK; }
    if (upper == "RDB$KEYWORDS") { getRdbKeywordsColumns(columns); return Status::OK; }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Unknown Firebird system table: " + table_name).c_str());
    return Status::NOT_FOUND;
}

// ============================================================================
// RDB$ Table Queries
// ============================================================================

Status FirebirdCatalogHandler::queryRdbDatabase(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$DESCRIPTION", "RDB$RELATION_ID", "RDB$SECURITY_CLASS",
        "RDB$CHARACTER_SET_NAME", "RDB$LINGER", "RDB$SQL_SECURITY"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT64, DataType::TEXT,
        DataType::TEXT, DataType::INT64, DataType::BOOLEAN
    };

    VirtualRow row;
    row.columns = {
        {"RDB$DESCRIPTION", TypedValue()},  // NULL
        {"RDB$RELATION_ID", TypedValue::makeInt64(0)},
        {"RDB$SECURITY_CLASS", TypedValue()},  // NULL
        {"RDB$CHARACTER_SET_NAME", TypedValue::makeVarchar("UTF8")},
        {"RDB$LINGER", TypedValue::makeInt64(0)},
        {"RDB$SQL_SECURITY", TypedValue::makeBool(true)}
    };
    results.rows.push_back(row);

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbRelations(VirtualResultSet& results, ErrorContext* ctx) {
    results.column_names = {
        "RDB$RELATION_NAME", "RDB$SYSTEM_FLAG", "RDB$RELATION_TYPE",
        "RDB$OWNER_NAME", "RDB$DESCRIPTION", "RDB$VIEW_SOURCE",
        "RDB$EXTERNAL_FILE", "RDB$RELATION_ID"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16, DataType::INT16,
        DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::TEXT, DataType::INT64
    };

    // Query catalog for all tables across all schemas
    if (!catalog_manager_) {
        // Return empty result if no catalog manager - this is valid for initial setup
        return Status::OK;
    }

    // Get all schemas first
    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    int64_t relationId = 128;  // Start after system tables
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
        if (status != Status::OK) {
            continue;  // Skip schemas we can't read
        }

        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"RDB$RELATION_NAME", TypedValue::makeVarchar(table.table_name)},
                {"RDB$SYSTEM_FLAG", TypedValue::makeInt64(0)},  // User table
                {"RDB$RELATION_TYPE", TypedValue::makeInt64(0)},  // 0 = table
                {"RDB$OWNER_NAME", TypedValue::makeVarchar("SYSDBA")},
                {"RDB$DESCRIPTION", TypedValue()},  // NULL
                {"RDB$VIEW_SOURCE", TypedValue()},  // NULL (not a view)
                {"RDB$EXTERNAL_FILE", TypedValue()},  // NULL
                {"RDB$RELATION_ID", TypedValue::makeInt64(relationId++)}
            };
            results.rows.push_back(row);
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbFields(VirtualResultSet& results, ErrorContext* /* ctx */) {
    // Domain definitions - minimal implementation
    results.column_names = {
        "RDB$FIELD_NAME", "RDB$FIELD_TYPE", "RDB$FIELD_LENGTH",
        "RDB$FIELD_SCALE", "RDB$CHARACTER_SET_ID", "RDB$COLLATION_ID",
        "RDB$NULL_FLAG", "RDB$DEFAULT_SOURCE", "RDB$DESCRIPTION"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16, DataType::INT16,
        DataType::INT16, DataType::INT16, DataType::INT16,
        DataType::INT16, DataType::TEXT, DataType::TEXT
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    struct FieldMapping {
        int16_t field_type = 0;
        int16_t field_length = 0;
        int16_t field_scale = 0;
        int16_t charset_id = 0;
        int16_t collation_id = 0;
    };

    auto mapDataType = [](DataType type, uint32_t precision, uint32_t scale) -> FieldMapping {
        FieldMapping mapping{};

        switch (type) {
            case DataType::INT8:
            case DataType::INT16:
                mapping.field_type = 7; // SMALLINT
                mapping.field_length = 2;
                break;
            case DataType::INT32:
                mapping.field_type = 8; // INTEGER
                mapping.field_length = 4;
                break;
            case DataType::INT64:
            case DataType::UINT64:
            case DataType::UINT32:
            case DataType::UINT16:
            case DataType::UINT8:
                mapping.field_type = 16; // BIGINT
                mapping.field_length = 8;
                break;
            case DataType::FLOAT32:
                mapping.field_type = 10; // FLOAT
                mapping.field_length = 4;
                break;
            case DataType::FLOAT64:
                mapping.field_type = 27; // DOUBLE
                mapping.field_length = 8;
                break;
            case DataType::DECIMAL:
            case DataType::MONEY: {
                uint32_t prec = precision == 0 ? 18 : precision;
                if (prec <= 4) {
                    mapping.field_type = 7;
                    mapping.field_length = 2;
                } else if (prec <= 9) {
                    mapping.field_type = 8;
                    mapping.field_length = 4;
                } else {
                    mapping.field_type = 16;
                    mapping.field_length = 8;
                }
                mapping.field_scale = static_cast<int16_t>(-static_cast<int32_t>(scale));
                break;
            }
            case DataType::CHAR:
                mapping.field_type = 14; // CHAR
                mapping.field_length = static_cast<int16_t>(precision == 0 ? 1 : precision);
                break;
            case DataType::VARCHAR:
                mapping.field_type = 37; // VARCHAR
                mapping.field_length = static_cast<int16_t>(precision == 0 ? 255 : precision);
                break;
            case DataType::TEXT:
                mapping.field_type = 37; // VARCHAR
                mapping.field_length = static_cast<int16_t>(precision == 0 ? 8192 : precision);
                break;
            case DataType::DATE:
                mapping.field_type = 12;
                mapping.field_length = 4;
                break;
            case DataType::TIME:
                mapping.field_type = 13;
                mapping.field_length = 4;
                break;
            case DataType::TIMESTAMP:
                mapping.field_type = 35;
                mapping.field_length = 8;
                break;
            case DataType::BOOLEAN:
                mapping.field_type = 23;
                mapping.field_length = 1;
                break;
            case DataType::UUID:
                mapping.field_type = 14; // CHAR
                mapping.field_length = 16;
                break;
            case DataType::BLOB:
            case DataType::BYTEA:
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::JSON:
            case DataType::JSONB:
            case DataType::XML:
            case DataType::ARRAY:
            case DataType::COMPOSITE:
            case DataType::VECTOR:
                mapping.field_type = 261; // BLOB
                mapping.field_length = 0;
                break;
            default:
                mapping.field_type = 261; // Default to BLOB
                mapping.field_length = 0;
                break;
        }

        return mapping;
    };

    auto addFieldRow = [&results](const std::string& field_name,
                                  const FieldMapping& mapping,
                                  bool nullable,
                                  const std::string& default_source) {
        VirtualRow row;
        row.columns = {
            {"RDB$FIELD_NAME", TypedValue::makeVarchar(field_name)},
            {"RDB$FIELD_TYPE", TypedValue::makeInt64(mapping.field_type)},
            {"RDB$FIELD_LENGTH", TypedValue::makeInt64(mapping.field_length)},
            {"RDB$FIELD_SCALE", TypedValue::makeInt64(mapping.field_scale)},
            {"RDB$CHARACTER_SET_ID", TypedValue::makeInt64(mapping.charset_id)},
            {"RDB$COLLATION_ID", TypedValue::makeInt64(mapping.collation_id)},
            {"RDB$NULL_FLAG", nullable ? TypedValue() : TypedValue::makeInt64(1)},
            {"RDB$DEFAULT_SOURCE", default_source.empty() ? TypedValue() : TypedValue::makeText(default_source)},
            {"RDB$DESCRIPTION", TypedValue()}
        };
        results.rows.push_back(std::move(row));
    };

    ID zero_id{};
    std::vector<DomainInfo> domains;
    std::unordered_map<ID, DomainInfo, IDHash> domain_map;
    if (catalog_manager_->listDomains(zero_id, domains, nullptr) == Status::OK) {
        for (const auto& domain : domains) {
            domain_map.emplace(domain.domain_id, domain);
        }
    }

    std::unordered_set<std::string> field_names;

    for (const auto& domain : domains) {
        std::string field_name = toUpperCase(domain.domain_name);
        if (!field_names.insert(field_name).second) {
            continue;
        }

        DataType base_type = domain.base_type;
        uint32_t precision = domain.precision;
        uint32_t scale = domain.scale;

        if (domain.domain_type == DomainType::ENUM) {
            base_type = DataType::VARCHAR;
            size_t max_len = 0;
            for (const auto& val : domain.enum_values) {
                max_len = std::max(max_len, val.label.size());
            }
            precision = max_len == 0 ? 1 : static_cast<uint32_t>(max_len);
            scale = 0;
        } else if (domain.domain_type != DomainType::BASIC) {
            base_type = DataType::BLOB;
        }

        FieldMapping mapping = mapDataType(base_type, precision, scale);
        addFieldRow(field_name, mapping, domain.nullable, domain.default_value);
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, nullptr);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        status = catalog_manager_->listTables(schema.schema_id, tables, nullptr);
        if (status != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::ColumnInfo> columns;
            status = catalog_manager_->getColumns(table.table_id, columns, nullptr);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& column : columns) {
                std::string field_name = toUpperCase(column.column_name);
                if (!field_names.insert(field_name).second) {
                    continue;
                }

                FieldMapping mapping{};
                auto domain_it = domain_map.find(column.domain_id);
                if (domain_it != domain_map.end()) {
                    const DomainInfo& domain = domain_it->second;
                    DataType base_type = domain.base_type;
                    uint32_t precision = domain.precision;
                    uint32_t scale = domain.scale;

                    if (domain.domain_type == DomainType::ENUM) {
                        base_type = DataType::VARCHAR;
                        size_t max_len = 0;
                        for (const auto& val : domain.enum_values) {
                            max_len = std::max(max_len, val.label.size());
                        }
                        precision = max_len == 0 ? 1 : static_cast<uint32_t>(max_len);
                        scale = 0;
                    } else if (domain.domain_type != DomainType::BASIC) {
                        base_type = DataType::BLOB;
                    }

                    mapping = mapDataType(base_type, precision, scale);
                } else {
                    mapping = mapDataType(static_cast<DataType>(column.data_type),
                                          column.type_precision,
                                          column.type_scale);
                }

                addFieldRow(field_name, mapping, column.nullable, column.default_value);
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbRelationFields(VirtualResultSet& results, ErrorContext* ctx) {
    results.column_names = {
        "RDB$RELATION_NAME", "RDB$FIELD_NAME", "RDB$FIELD_SOURCE",
        "RDB$FIELD_POSITION", "RDB$NULL_FLAG", "RDB$DEFAULT_SOURCE",
        "RDB$DESCRIPTION", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::INT16, DataType::INT16, DataType::TEXT,
        DataType::TEXT, DataType::INT16
    };

    if (!catalog_manager_) {
        // Return empty result if no catalog manager
        return Status::OK;
    }

    // Get all schemas first
    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
        if (status != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::ColumnInfo> columns;
            status = catalog_manager_->getColumns(table.table_id, columns, ctx);
            if (status != Status::OK) {
                continue;
            }

            int64_t position = 0;
            for (const auto& col : columns) {
                VirtualRow row;
                row.columns = {
                    {"RDB$RELATION_NAME", TypedValue::makeVarchar(table.table_name)},
                    {"RDB$FIELD_NAME", TypedValue::makeVarchar(col.column_name)},
                    {"RDB$FIELD_SOURCE", TypedValue::makeVarchar(col.column_name)},  // Field source = column name
                    {"RDB$FIELD_POSITION", TypedValue::makeInt64(position++)},
                    {"RDB$NULL_FLAG", col.nullable ? TypedValue() : TypedValue::makeInt64(1)},
                    {"RDB$DEFAULT_SOURCE", TypedValue()},  // NULL
                    {"RDB$DESCRIPTION", TypedValue()},  // NULL
                    {"RDB$SYSTEM_FLAG", TypedValue::makeInt64(0)}
                };
                results.rows.push_back(row);
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbIndices(VirtualResultSet& results, ErrorContext* ctx) {
    results.column_names = {
        "RDB$INDEX_NAME", "RDB$RELATION_NAME", "RDB$UNIQUE_FLAG",
        "RDB$INDEX_INACTIVE", "RDB$INDEX_TYPE", "RDB$SEGMENT_COUNT",
        "RDB$DESCRIPTION", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::INT16,
        DataType::INT16, DataType::INT16, DataType::INT16,
        DataType::TEXT, DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
        if (status != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::IndexInfo> indexes;
            status = catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& index : indexes) {
                VirtualRow row;
                row.columns = {
                    {"RDB$INDEX_NAME", TypedValue::makeVarchar(index.index_name)},
                    {"RDB$RELATION_NAME", TypedValue::makeVarchar(table.table_name)},
                    {"RDB$UNIQUE_FLAG", TypedValue::makeInt16(index.is_unique ? 1 : 0)},
                    {"RDB$INDEX_INACTIVE", TypedValue::makeInt16(0)},
                    {"RDB$INDEX_TYPE", TypedValue::makeInt16(0)},
                    {"RDB$SEGMENT_COUNT", TypedValue::makeInt16(static_cast<int16_t>(index.column_ids.size()))},
                    {"RDB$DESCRIPTION", TypedValue()},
                    {"RDB$SYSTEM_FLAG", TypedValue::makeInt16(0)}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbIndexSegments(VirtualResultSet& results, ErrorContext* ctx) {
    results.column_names = {
        "RDB$INDEX_NAME", "RDB$FIELD_NAME", "RDB$FIELD_POSITION"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
        if (status != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::ColumnInfo> columns;
            status = catalog_manager_->getColumns(table.table_id, columns, ctx);
            if (status != Status::OK) {
                continue;
            }

            std::unordered_map<ID, std::string, IDHash> column_names;
            for (const auto& col : columns) {
                column_names[col.column_id] = col.column_name;
            }

            std::vector<CatalogManager::IndexInfo> indexes;
            status = catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& index : indexes) {
                for (size_t pos = 0; pos < index.column_ids.size(); ++pos) {
                    const auto& col_id = index.column_ids[pos];
                    auto it = column_names.find(col_id);
                    std::string col_name = (it != column_names.end())
                        ? it->second
                        : ("COLUMN_" + std::to_string(pos));

                    VirtualRow row;
                    row.columns = {
                        {"RDB$INDEX_NAME", TypedValue::makeVarchar(index.index_name)},
                        {"RDB$FIELD_NAME", TypedValue::makeVarchar(col_name)},
                        {"RDB$FIELD_POSITION", TypedValue::makeInt16(static_cast<int16_t>(pos))}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbGenerators(VirtualResultSet& results, ErrorContext* ctx) {
    results.column_names = {
        "RDB$GENERATOR_NAME", "RDB$GENERATOR_ID", "RDB$SYSTEM_FLAG",
        "RDB$DESCRIPTION", "RDB$OWNER_NAME", "RDB$INITIAL_VALUE",
        "RDB$GENERATOR_INCREMENT"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT64, DataType::INT16,
        DataType::TEXT, DataType::TEXT, DataType::INT64,
        DataType::INT64
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    int64_t generator_id = 0;
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::SequenceInfo> sequences;
        status = catalog_manager_->listSequences(schema.schema_id, sequences, ctx);
        if (status != Status::OK) {
            continue;
        }

        for (const auto& seq : sequences) {
            VirtualRow row;
            row.columns = {
                {"RDB$GENERATOR_NAME", TypedValue::makeVarchar(seq.name)},
                {"RDB$GENERATOR_ID", TypedValue::makeInt64(generator_id++)},
                {"RDB$SYSTEM_FLAG", TypedValue::makeInt16(0)},
                {"RDB$DESCRIPTION", TypedValue()},
                {"RDB$OWNER_NAME", TypedValue::makeVarchar("SYSDBA")},
                {"RDB$INITIAL_VALUE", TypedValue::makeInt64(seq.start_value)},
                {"RDB$GENERATOR_INCREMENT", TypedValue::makeInt64(seq.increment_by)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbProcedures(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$PROCEDURE_NAME", "RDB$PROCEDURE_ID", "RDB$PROCEDURE_INPUTS",
        "RDB$PROCEDURE_OUTPUTS", "RDB$DESCRIPTION", "RDB$PROCEDURE_SOURCE",
        "RDB$OWNER_NAME", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT64, DataType::INT16,
        DataType::INT16, DataType::TEXT, DataType::TEXT,
        DataType::TEXT, DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::ProcedureInfo> procedures;
    if (catalog_manager_->listProcedures(procedures, nullptr) != Status::OK) {
        return Status::OK;
    }

    int64_t proc_id = 0;
    for (const auto& proc : procedures) {
        int16_t inputs = 0;
        int16_t outputs = 0;
        for (const auto& param : proc.parameters) {
            if (param.mode == CatalogManager::ParameterMode::IN ||
                param.mode == CatalogManager::ParameterMode::INOUT) {
                ++inputs;
            }
            if (param.mode == CatalogManager::ParameterMode::OUT) {
                ++outputs;
            }
        }

        VirtualRow row;
        row.columns = {
            {"RDB$PROCEDURE_NAME", TypedValue::makeVarchar(proc.name)},
            {"RDB$PROCEDURE_ID", TypedValue::makeInt64(proc_id++)},
            {"RDB$PROCEDURE_INPUTS", TypedValue::makeInt16(inputs)},
            {"RDB$PROCEDURE_OUTPUTS", TypedValue::makeInt16(outputs)},
            {"RDB$DESCRIPTION", TypedValue()},
            {"RDB$PROCEDURE_SOURCE", proc.source_text.empty() ? TypedValue()
                                                              : TypedValue::makeText(proc.source_text)},
            {"RDB$OWNER_NAME", TypedValue::makeVarchar("SYSDBA")},
            {"RDB$SYSTEM_FLAG", TypedValue::makeInt16(0)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbProcedureParameters(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$PROCEDURE_NAME", "RDB$PARAMETER_NAME", "RDB$PARAMETER_TYPE",
        "RDB$FIELD_SOURCE", "RDB$DEFAULT_SOURCE", "RDB$DESCRIPTION",
        "RDB$PARAMETER_NUMBER"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::INT16,
        DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::ProcedureInfo> procedures;
    if (catalog_manager_->listProcedures(procedures, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& proc : procedures) {
        int16_t param_index = 0;
        for (const auto& param : proc.parameters) {
            int16_t param_type = (param.mode == CatalogManager::ParameterMode::OUT) ? 1 : 0;
            VirtualRow row;
            row.columns = {
                {"RDB$PROCEDURE_NAME", TypedValue::makeVarchar(proc.name)},
                {"RDB$PARAMETER_NAME", TypedValue::makeVarchar(param.name)},
                {"RDB$PARAMETER_TYPE", TypedValue::makeInt16(param_type)},
                {"RDB$FIELD_SOURCE", TypedValue::makeVarchar(param.name)},
                {"RDB$DEFAULT_SOURCE", param.has_default ? TypedValue::makeText(param.default_value)
                                                         : TypedValue()},
                {"RDB$DESCRIPTION", TypedValue()},
                {"RDB$PARAMETER_NUMBER", TypedValue::makeInt16(param_index++)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbFunctions(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$FUNCTION_NAME", "RDB$FUNCTION_TYPE", "RDB$RETURN_ARGUMENT",
        "RDB$DESCRIPTION", "RDB$MODULE_NAME", "RDB$ENTRYPOINT",
        "RDB$OWNER_NAME", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16, DataType::INT16,
        DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::TEXT, DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::FunctionInfo> functions;
    if (catalog_manager_->listFunctions(functions, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& func : functions) {
        VirtualRow row;
        row.columns = {
            {"RDB$FUNCTION_NAME", TypedValue::makeVarchar(func.name)},
            {"RDB$FUNCTION_TYPE", TypedValue::makeInt16(0)},
            {"RDB$RETURN_ARGUMENT", TypedValue::makeInt16(0)},
            {"RDB$DESCRIPTION", TypedValue()},
            {"RDB$MODULE_NAME", TypedValue()},
            {"RDB$ENTRYPOINT", TypedValue()},
            {"RDB$OWNER_NAME", TypedValue::makeVarchar("SYSDBA")},
            {"RDB$SYSTEM_FLAG", TypedValue::makeInt16(0)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbFunctionArguments(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$FUNCTION_NAME", "RDB$ARGUMENT_NAME", "RDB$ARGUMENT_POSITION",
        "RDB$MECHANISM", "RDB$FIELD_SOURCE"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::INT16,
        DataType::INT16, DataType::TEXT
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::FunctionInfo> functions;
    if (catalog_manager_->listFunctions(functions, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& func : functions) {
        int16_t arg_pos = 0;
        for (const auto& param : func.parameters) {
            VirtualRow row;
            row.columns = {
                {"RDB$FUNCTION_NAME", TypedValue::makeVarchar(func.name)},
                {"RDB$ARGUMENT_NAME", TypedValue::makeVarchar(param.name)},
                {"RDB$ARGUMENT_POSITION", TypedValue::makeInt16(arg_pos++)},
                {"RDB$MECHANISM", TypedValue::makeInt16(0)},
                {"RDB$FIELD_SOURCE", TypedValue::makeVarchar(param.name)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbTriggers(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$TRIGGER_NAME", "RDB$RELATION_NAME", "RDB$TRIGGER_SEQUENCE",
        "RDB$TRIGGER_TYPE", "RDB$TRIGGER_SOURCE", "RDB$DESCRIPTION",
        "RDB$TRIGGER_INACTIVE", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::INT16,
        DataType::INT64, DataType::TEXT, DataType::TEXT,
        DataType::INT16, DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, nullptr) != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::TriggerInfo> triggers;
            if (catalog_manager_->listAllTriggersForTable(table.table_id, triggers, nullptr) != Status::OK) {
                continue;
            }

            int16_t seq = 0;
            for (const auto& trigger : triggers) {
                auto has_event = [&](CatalogManager::TriggerEvent event) {
                    return (trigger.event_mask &
                            (1u << static_cast<uint8_t>(event))) != 0;
                };

                auto emit_trigger = [&](int32_t trigger_type) {
                    VirtualRow row;
                    row.columns = {
                        {"RDB$TRIGGER_NAME", TypedValue::makeVarchar(trigger.trigger_name)},
                        {"RDB$RELATION_NAME", TypedValue::makeVarchar(table.table_name)},
                        {"RDB$TRIGGER_SEQUENCE", TypedValue::makeInt16(seq++)},
                        {"RDB$TRIGGER_TYPE", TypedValue::makeInt64(trigger_type)},
                        {"RDB$TRIGGER_SOURCE", TypedValue()},
                        {"RDB$DESCRIPTION", TypedValue()},
                        {"RDB$TRIGGER_INACTIVE", TypedValue::makeInt16(trigger.enabled ? 0 : 1)},
                        {"RDB$SYSTEM_FLAG", TypedValue::makeInt16(0)}
                    };
                    results.rows.push_back(std::move(row));
                };

                if (trigger.timing == CatalogManager::TriggerTiming::BEFORE) {
                    if (has_event(CatalogManager::TriggerEvent::INSERT)) emit_trigger(1);
                    if (has_event(CatalogManager::TriggerEvent::UPDATE)) emit_trigger(3);
                    if (has_event(CatalogManager::TriggerEvent::DELETE)) emit_trigger(5);
                } else if (trigger.timing == CatalogManager::TriggerTiming::AFTER) {
                    if (has_event(CatalogManager::TriggerEvent::INSERT)) emit_trigger(2);
                    if (has_event(CatalogManager::TriggerEvent::UPDATE)) emit_trigger(4);
                    if (has_event(CatalogManager::TriggerEvent::DELETE)) emit_trigger(6);
                }
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbConstraints(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$CONSTRAINT_NAME", "RDB$CONSTRAINT_TYPE", "RDB$RELATION_NAME",
        "RDB$INDEX_NAME", "RDB$DEFERRABLE", "RDB$INITIALLY_DEFERRED"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::TEXT, DataType::TEXT, DataType::TEXT
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    auto constraintTypeName = [](CatalogManager::ConstraintType type) -> std::string {
        switch (type) {
            case CatalogManager::ConstraintType::PRIMARY_KEY: return "PRIMARY KEY";
            case CatalogManager::ConstraintType::UNIQUE: return "UNIQUE";
            case CatalogManager::ConstraintType::FOREIGN_KEY: return "FOREIGN KEY";
            case CatalogManager::ConstraintType::CHECK: return "CHECK";
            case CatalogManager::ConstraintType::NOT_NULL: return "NOT NULL";
            case CatalogManager::ConstraintType::EXCLUSION: return "EXCLUSION";
        }
        return "UNKNOWN";
    };

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, nullptr) != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::ConstraintInfo> constraints;
            if (catalog_manager_->getConstraintsForTable(table.table_id, constraints, nullptr) != Status::OK) {
                continue;
            }
            for (const auto& constraint : constraints) {
                VirtualRow row;
                row.columns = {
                    {"RDB$CONSTRAINT_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                    {"RDB$CONSTRAINT_TYPE", TypedValue::makeVarchar(constraintTypeName(constraint.constraint_type))},
                    {"RDB$RELATION_NAME", TypedValue::makeVarchar(table.table_name)},
                    {"RDB$INDEX_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                    {"RDB$DEFERRABLE", TypedValue::makeVarchar(constraint.is_deferrable ? "YES" : "NO")},
                    {"RDB$INITIALLY_DEFERRED", TypedValue::makeVarchar(constraint.initially_deferred ? "YES" : "NO")}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbRelationConstraints(VirtualResultSet& results, ErrorContext* ctx) {
    return queryRdbConstraints(results, ctx);
}

Status FirebirdCatalogHandler::queryRdbCheckConstraints(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$CONSTRAINT_NAME", "RDB$TRIGGER_NAME"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, nullptr) != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::ConstraintInfo> constraints;
            if (catalog_manager_->getConstraintsForTable(table.table_id, constraints, nullptr) != Status::OK) {
                continue;
            }
            for (const auto& constraint : constraints) {
                if (constraint.constraint_type != CatalogManager::ConstraintType::CHECK) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"RDB$CONSTRAINT_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                    {"RDB$TRIGGER_NAME", TypedValue::makeVarchar(constraint.constraint_name)}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbRefConstraints(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$CONSTRAINT_NAME", "RDB$CONST_NAME_UQ", "RDB$MATCH_OPTION",
        "RDB$UPDATE_RULE", "RDB$DELETE_RULE"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::TEXT, DataType::TEXT
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    auto fkActionName = [](CatalogManager::FKAction action) -> std::string {
        switch (action) {
            case CatalogManager::FKAction::NO_ACTION: return "NO ACTION";
            case CatalogManager::FKAction::RESTRICT: return "RESTRICT";
            case CatalogManager::FKAction::CASCADE: return "CASCADE";
            case CatalogManager::FKAction::SET_NULL: return "SET NULL";
            case CatalogManager::FKAction::SET_DEFAULT: return "SET DEFAULT";
        }
        return "NO ACTION";
    };

    auto fkMatchName = [](CatalogManager::FKMatchType match) -> std::string {
        switch (match) {
            case CatalogManager::FKMatchType::FULL: return "FULL";
            case CatalogManager::FKMatchType::PARTIAL: return "PARTIAL";
            case CatalogManager::FKMatchType::SIMPLE:
            default: return "SIMPLE";
        }
    };

    auto findMatchingConstraint = [&](const CatalogManager::ConstraintInfo& fk) -> std::string {
        if (fk.referenced_table_id == ID{}) {
            return {};
        }
        std::vector<CatalogManager::ConstraintInfo> target;
        if (catalog_manager_->getConstraintsByType(
                fk.referenced_table_id,
                CatalogManager::ConstraintType::PRIMARY_KEY,
                target, nullptr) != Status::OK) {
            target.clear();
        }
        if (target.empty()) {
            catalog_manager_->getConstraintsByType(
                fk.referenced_table_id,
                CatalogManager::ConstraintType::UNIQUE,
                target, nullptr);
        }
        for (const auto& c : target) {
            if (c.column_names == fk.referenced_columns) {
                return c.constraint_name;
            }
        }
        return {};
    };

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, nullptr) != Status::OK) {
            continue;
        }

        for (const auto& table : tables) {
            std::vector<CatalogManager::ConstraintInfo> constraints;
            if (catalog_manager_->getConstraintsForTable(table.table_id, constraints, nullptr) != Status::OK) {
                continue;
            }
            for (const auto& constraint : constraints) {
                if (constraint.constraint_type != CatalogManager::ConstraintType::FOREIGN_KEY) {
                    continue;
                }
                std::string ref_name = findMatchingConstraint(constraint);
                VirtualRow row;
                row.columns = {
                    {"RDB$CONSTRAINT_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                    {"RDB$CONST_NAME_UQ", ref_name.empty() ? TypedValue() : TypedValue::makeVarchar(ref_name)},
                    {"RDB$MATCH_OPTION", TypedValue::makeVarchar(fkMatchName(constraint.match_type))},
                    {"RDB$UPDATE_RULE", TypedValue::makeVarchar(fkActionName(constraint.on_update))},
                    {"RDB$DELETE_RULE", TypedValue::makeVarchar(fkActionName(constraint.on_delete))}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbViewRelations(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {"RDB$VIEW_NAME", "RDB$RELATION_NAME"};
    results.column_types = {DataType::TEXT, DataType::TEXT};

    if (!catalog_manager_) {
        return Status::OK;
    }

    auto resolveTableName = [&](const core::ID& table_id) -> std::string {
        CatalogManager::TableInfo table_info;
        if (catalog_manager_->getTable(table_id, table_info, nullptr) == Status::OK) {
            return table_info.table_name;
        }
        return {};
    };

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) != Status::OK) {
        return Status::OK;
    }

    std::vector<CatalogManager::DependencyInfo> deps;
    catalog_manager_->listDependencies(deps, nullptr);

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::ViewInfo> views;
        if (catalog_manager_->listViewsForSchema(schema.schema_id, views, nullptr) != Status::OK) {
            continue;
        }

        for (const auto& view : views) {
            bool emitted = false;
            if (!view.base_table_ids.empty()) {
                for (const auto& base_id : view.base_table_ids) {
                    std::string rel_name = resolveTableName(base_id);
                    VirtualRow row;
                    row.columns = {
                        {"RDB$VIEW_NAME", TypedValue::makeVarchar(view.name)},
                        {"RDB$RELATION_NAME", TypedValue::makeVarchar(rel_name)}
                    };
                    results.rows.push_back(std::move(row));
                    emitted = true;
                }
            }

            for (const auto& dep : deps) {
                if (dep.dependent_type != CatalogManager::ObjectType::VIEW ||
                    dep.dependent_object_id != view.view_id ||
                    dep.referenced_type != CatalogManager::ObjectType::TABLE) {
                    continue;
                }
                std::string rel_name = resolveTableName(dep.referenced_object_id);
                VirtualRow row;
                row.columns = {
                    {"RDB$VIEW_NAME", TypedValue::makeVarchar(view.name)},
                    {"RDB$RELATION_NAME", TypedValue::makeVarchar(rel_name)}
                };
                results.rows.push_back(std::move(row));
                emitted = true;
            }

            if (!emitted) {
                VirtualRow row;
                row.columns = {
                    {"RDB$VIEW_NAME", TypedValue::makeVarchar(view.name)},
                    {"RDB$RELATION_NAME", TypedValue()}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbExceptions(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$EXCEPTION_NAME", "RDB$MESSAGE", "RDB$DESCRIPTION", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::ExceptionInfo> exceptions;
        if (catalog_manager_->listExceptions(schema.schema_id, exceptions, nullptr) != Status::OK) {
            continue;
        }
        for (const auto& ex : exceptions) {
            VirtualRow row;
            row.columns = {
                {"RDB$EXCEPTION_NAME", TypedValue::makeVarchar(ex.name)},
                {"RDB$MESSAGE", TypedValue::makeVarchar(ex.message)},
                {"RDB$DESCRIPTION", TypedValue()},
                {"RDB$SYSTEM_FLAG", TypedValue::makeInt16(0)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbDependencies(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$DEPENDENT_NAME", "RDB$DEPENDENT_TYPE",
        "RDB$DEPENDED_ON_NAME", "RDB$DEPENDED_ON_TYPE",
        "RDB$FIELD_NAME"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16,
        DataType::TEXT, DataType::INT16,
        DataType::TEXT
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::ProcedureInfo> procedures;
    catalog_manager_->listProcedures(procedures, nullptr);
    std::vector<CatalogManager::ViewInfo> all_views;
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) == Status::OK) {
        for (const auto& schema : schemas) {
            std::vector<CatalogManager::ViewInfo> views;
            if (catalog_manager_->listViewsForSchema(schema.schema_id, views, nullptr) == Status::OK) {
                all_views.insert(all_views.end(), views.begin(), views.end());
            }
        }
    }

    auto resolveObjectName = [&](const core::ID& object_id,
                                 CatalogManager::ObjectType type) -> std::string {
        switch (type) {
            case CatalogManager::ObjectType::SCHEMA: {
                CatalogManager::SchemaInfo info;
                if (catalog_manager_->getSchema(object_id, info, nullptr) == Status::OK) {
                    return info.full_path.empty() ? info.schema_name : info.full_path;
                }
                break;
            }
            case CatalogManager::ObjectType::TABLE: {
                CatalogManager::TableInfo info;
                if (catalog_manager_->getTable(object_id, info, nullptr) == Status::OK) {
                    return info.table_name;
                }
                break;
            }
            case CatalogManager::ObjectType::VIEW: {
                for (const auto& view : all_views) {
                    if (view.view_id == object_id) {
                        return view.name;
                    }
                }
                break;
            }
            case CatalogManager::ObjectType::INDEX: {
                CatalogManager::IndexInfo info;
                if (catalog_manager_->getIndex(object_id, info, nullptr) == Status::OK) {
                    return info.index_name;
                }
                break;
            }
            case CatalogManager::ObjectType::SEQUENCE: {
                CatalogManager::SequenceInfo info;
                if (catalog_manager_->getSequenceById(object_id, info, nullptr) == Status::OK) {
                    return info.name;
                }
                break;
            }
            case CatalogManager::ObjectType::CONSTRAINT: {
                CatalogManager::ConstraintInfo info;
                if (catalog_manager_->getConstraint(object_id, info, nullptr) == Status::OK) {
                    return info.constraint_name;
                }
                break;
            }
            case CatalogManager::ObjectType::TRIGGER: {
                CatalogManager::TriggerInfo info;
                if (catalog_manager_->getTrigger(object_id, info, nullptr) == Status::OK) {
                    return info.trigger_name;
                }
                break;
            }
            case CatalogManager::ObjectType::FUNCTION: {
                CatalogManager::FunctionInfo info;
                if (catalog_manager_->getFunctionById(object_id, info, nullptr) == Status::OK) {
                    return info.name;
                }
                break;
            }
            case CatalogManager::ObjectType::PROCEDURE: {
                for (const auto& proc : procedures) {
                    if (proc.procedure_id == object_id) {
                        return proc.name;
                    }
                }
                break;
            }
            case CatalogManager::ObjectType::DOMAIN: {
                DomainInfo info;
                if (catalog_manager_->getDomainById(object_id, info, nullptr) == Status::OK) {
                    return info.domain_name;
                }
                break;
            }
            case CatalogManager::ObjectType::ROLE: {
                CatalogManager::RoleInfo info;
                if (catalog_manager_->getRole(object_id, info, nullptr) == Status::OK) {
                    return info.role_name;
                }
                break;
            }
            case CatalogManager::ObjectType::USER: {
                CatalogManager::UserInfo info;
                if (catalog_manager_->getUser(object_id, info, nullptr) == Status::OK) {
                    return info.username;
                }
                break;
            }
            case CatalogManager::ObjectType::GROUP: {
                CatalogManager::GroupInfo info;
                if (catalog_manager_->getGroup(object_id, info, nullptr) == Status::OK) {
                    return info.group_name;
                }
                break;
            }
            case CatalogManager::ObjectType::PACKAGE: {
                CatalogManager::PackageInfo info;
                if (catalog_manager_->getPackage(object_id, info, nullptr) == Status::OK) {
                    return info.package_name;
                }
                break;
            }
            case CatalogManager::ObjectType::EXCEPTION: {
                CatalogManager::ExceptionInfo info;
                if (catalog_manager_->getException(object_id, info, nullptr) == Status::OK) {
                    return info.name;
                }
                break;
            }
            default:
                break;
        }

        return {};
    };

    std::vector<CatalogManager::DependencyInfo> deps;
    if (catalog_manager_->listDependencies(deps, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& dep : deps) {
        std::string dependent_name = resolveObjectName(dep.dependent_object_id, dep.dependent_type);
        std::string referenced_name = resolveObjectName(dep.referenced_object_id, dep.referenced_type);
        VirtualRow row;
        row.columns = {
            {"RDB$DEPENDENT_NAME", TypedValue::makeVarchar(dependent_name)},
            {"RDB$DEPENDENT_TYPE", TypedValue::makeInt16(static_cast<int16_t>(dep.dependent_type))},
            {"RDB$DEPENDED_ON_NAME", TypedValue::makeVarchar(referenced_name)},
            {"RDB$DEPENDED_ON_TYPE", TypedValue::makeInt16(static_cast<int16_t>(dep.referenced_type))},
            {"RDB$FIELD_NAME", TypedValue()}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbPackages(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$PACKAGE_NAME", "RDB$DESCRIPTION", "RDB$OWNER_NAME", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::INT16
    };

    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, nullptr) != Status::OK) {
        return Status::OK;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::PackageInfo> packages;
        if (catalog_manager_->listPackages(schema.schema_id, packages, nullptr) != Status::OK) {
            continue;
        }
        for (const auto& pkg : packages) {
            VirtualRow row;
            row.columns = {
                {"RDB$PACKAGE_NAME", TypedValue::makeVarchar(pkg.package_name)},
                {"RDB$DESCRIPTION", TypedValue()},
                {"RDB$OWNER_NAME", TypedValue::makeVarchar("SYSDBA")},
                {"RDB$SYSTEM_FLAG", TypedValue::makeInt16(0)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbKeywords(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$KEYWORD_NAME", "RDB$KEYWORD_TYPE"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16
    };

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbCharacterSets(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$CHARACTER_SET_NAME", "RDB$CHARACTER_SET_ID", "RDB$BYTES_PER_CHARACTER",
        "RDB$DEFAULT_COLLATE_NAME", "RDB$DESCRIPTION", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16, DataType::INT16,
        DataType::TEXT, DataType::TEXT, DataType::INT16
    };

    // Add common character sets
    const struct { const char* name; int id; int bytes; const char* collate; } charsets[] = {
        {"NONE", 0, 1, "NONE"},
        {"ASCII", 2, 1, "ASCII"},
        {"UTF8", 4, 4, "UTF8"},
        {"UNICODE_FSS", 3, 3, "UNICODE_FSS"},
        {"ISO8859_1", 21, 1, "ISO8859_1"},
        {"WIN1252", 53, 1, "WIN1252"}
    };

    for (const auto& cs : charsets) {
        VirtualRow row;
        row.columns = {
            {"RDB$CHARACTER_SET_NAME", TypedValue::makeVarchar(cs.name)},
            {"RDB$CHARACTER_SET_ID", TypedValue::makeInt64(cs.id)},
            {"RDB$BYTES_PER_CHARACTER", TypedValue::makeInt64(cs.bytes)},
            {"RDB$DEFAULT_COLLATE_NAME", TypedValue::makeVarchar(cs.collate)},
            {"RDB$DESCRIPTION", TypedValue()},  // NULL
            {"RDB$SYSTEM_FLAG", TypedValue::makeInt64(1)}  // System
        };
        results.rows.push_back(row);
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbCollations(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$COLLATION_NAME", "RDB$COLLATION_ID", "RDB$CHARACTER_SET_ID",
        "RDB$COLLATION_ATTRIBUTES", "RDB$DESCRIPTION", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16, DataType::INT16,
        DataType::INT16, DataType::TEXT, DataType::INT16
    };

    // Add default collations
    VirtualRow row;
    row.columns = {
        {"RDB$COLLATION_NAME", TypedValue::makeVarchar("UTF8")},
        {"RDB$COLLATION_ID", TypedValue::makeInt64(0)},
        {"RDB$CHARACTER_SET_ID", TypedValue::makeInt64(4)},
        {"RDB$COLLATION_ATTRIBUTES", TypedValue::makeInt64(0)},
        {"RDB$DESCRIPTION", TypedValue()},  // NULL
        {"RDB$SYSTEM_FLAG", TypedValue::makeInt64(1)}
    };
    results.rows.push_back(row);

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbTypes(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$TYPE_NAME", "RDB$TYPE", "RDB$FIELD_NAME",
        "RDB$DESCRIPTION", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT16, DataType::TEXT,
        DataType::TEXT, DataType::INT16
    };

    // Add basic types
    const struct { const char* name; int type; const char* field; } types[] = {
        {"TABLE", 0, "RDB$RELATION_TYPE"},
        {"VIEW", 1, "RDB$RELATION_TYPE"},
        {"SMALLINT", 7, "RDB$FIELD_TYPE"},
        {"INTEGER", 8, "RDB$FIELD_TYPE"},
        {"FLOAT", 10, "RDB$FIELD_TYPE"},
        {"DATE", 12, "RDB$FIELD_TYPE"},
        {"TIME", 13, "RDB$FIELD_TYPE"},
        {"VARCHAR", 37, "RDB$FIELD_TYPE"},
        {"BLOB", 261, "RDB$FIELD_TYPE"},
        {"BIGINT", 16, "RDB$FIELD_TYPE"},
        {"BOOLEAN", 23, "RDB$FIELD_TYPE"}
    };

    for (const auto& t : types) {
        VirtualRow row;
        row.columns = {
            {"RDB$TYPE_NAME", TypedValue::makeVarchar(t.name)},
            {"RDB$TYPE", TypedValue::makeInt64(t.type)},
            {"RDB$FIELD_NAME", TypedValue::makeVarchar(t.field)},
            {"RDB$DESCRIPTION", TypedValue()},  // NULL
            {"RDB$SYSTEM_FLAG", TypedValue::makeInt64(1)}
        };
        results.rows.push_back(row);
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbUserPrivileges(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$USER", "RDB$GRANTOR", "RDB$PRIVILEGE", "RDB$GRANT_OPTION",
        "RDB$RELATION_NAME", "RDB$FIELD_NAME", "RDB$USER_TYPE", "RDB$OBJECT_TYPE"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::INT16,
        DataType::TEXT, DataType::TEXT, DataType::INT16, DataType::INT16
    };

    // Privileges will be populated from security system
    return Status::OK;
}

Status FirebirdCatalogHandler::queryRdbRoles(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "RDB$ROLE_NAME", "RDB$OWNER_NAME", "RDB$DESCRIPTION", "RDB$SYSTEM_FLAG"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::INT16
    };

    // Add default roles
    VirtualRow row;
    row.columns = {
        {"RDB$ROLE_NAME", TypedValue::makeVarchar("ADMIN")},
        {"RDB$OWNER_NAME", TypedValue::makeVarchar("SYSDBA")},
        {"RDB$DESCRIPTION", TypedValue::makeVarchar("Administrator role")},
        {"RDB$SYSTEM_FLAG", TypedValue::makeInt64(1)}
    };
    results.rows.push_back(row);

    return Status::OK;
}

// ============================================================================
// MON$ Table Queries
// ============================================================================

Status FirebirdCatalogHandler::queryMonDatabase(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$DATABASE_NAME", "MON$PAGE_SIZE", "MON$ODS_MAJOR", "MON$ODS_MINOR",
        "MON$OLDEST_TRANSACTION", "MON$OLDEST_ACTIVE", "MON$OLDEST_SNAPSHOT",
        "MON$NEXT_TRANSACTION", "MON$PAGE_BUFFERS", "MON$SQL_DIALECT",
        "MON$SHUTDOWN_MODE", "MON$SWEEP_INTERVAL", "MON$READ_ONLY",
        "MON$FORCED_WRITES", "MON$RESERVE_SPACE", "MON$CREATION_DATE",
        "MON$PAGES", "MON$STAT_ID", "MON$BACKUP_STATE",
        "MON$CRYPT_PAGE", "MON$OWNER", "MON$SEC_DATABASE"
    };
    results.column_types = {
        DataType::TEXT, DataType::INT64, DataType::INT16, DataType::INT16,
        DataType::INT64, DataType::INT64, DataType::INT64,
        DataType::INT64, DataType::INT64, DataType::INT16,
        DataType::INT16, DataType::INT64, DataType::INT16,
        DataType::INT16, DataType::INT16, DataType::TIMESTAMP,
        DataType::INT64, DataType::INT64, DataType::INT16,
        DataType::INT64, DataType::TEXT, DataType::TEXT
    };

    VirtualRow row;
    row.columns = {
        {"MON$DATABASE_NAME", TypedValue::makeVarchar("scratchbird.sbdb")},
        {"MON$PAGE_SIZE", TypedValue::makeInt64(16384)},
        {"MON$ODS_MAJOR", TypedValue::makeInt64(13)},  // Firebird 5.0
        {"MON$ODS_MINOR", TypedValue::makeInt64(0)},
        {"MON$OLDEST_TRANSACTION", TypedValue::makeInt64(1)},
        {"MON$OLDEST_ACTIVE", TypedValue::makeInt64(1)},
        {"MON$OLDEST_SNAPSHOT", TypedValue::makeInt64(1)},
        {"MON$NEXT_TRANSACTION", TypedValue::makeInt64(100)},
        {"MON$PAGE_BUFFERS", TypedValue::makeInt64(2048)},
        {"MON$SQL_DIALECT", TypedValue::makeInt64(3)},
        {"MON$SHUTDOWN_MODE", TypedValue::makeInt64(0)},  // Online
        {"MON$SWEEP_INTERVAL", TypedValue::makeInt64(20000)},
        {"MON$READ_ONLY", TypedValue::makeInt64(0)},  // Read-write
        {"MON$FORCED_WRITES", TypedValue::makeInt64(1)},
        {"MON$RESERVE_SPACE", TypedValue::makeInt64(1)},
        {"MON$CREATION_DATE", TypedValue()},  // NULL
        {"MON$PAGES", TypedValue::makeInt64(1000)},
        {"MON$STAT_ID", TypedValue::makeInt64(1)},
        {"MON$BACKUP_STATE", TypedValue::makeInt64(0)},  // Normal
        {"MON$CRYPT_PAGE", TypedValue::makeInt64(0)},
        {"MON$OWNER", TypedValue::makeVarchar("SYSDBA")},
        {"MON$SEC_DATABASE", TypedValue::makeVarchar("Default")}
    };
    results.rows.push_back(row);

    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonAttachments(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$ATTACHMENT_ID", "MON$SERVER_PID", "MON$STATE", "MON$ATTACHMENT_NAME",
        "MON$USER", "MON$ROLE", "MON$REMOTE_PROTOCOL", "MON$REMOTE_ADDRESS",
        "MON$REMOTE_PID", "MON$CHARACTER_SET_ID", "MON$TIMESTAMP", "MON$GARBAGE_COLLECTION",
        "MON$REMOTE_PROCESS", "MON$STAT_ID", "MON$CLIENT_VERSION", "MON$REMOTE_VERSION"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::INT16, DataType::TEXT,
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::INT64, DataType::INT16, DataType::TIMESTAMP, DataType::INT16,
        DataType::TEXT, DataType::INT64, DataType::TEXT, DataType::TEXT
    };

    std::vector<ProcessControlBlock> backends;
    core::ErrorContext proc_ctx;
    if (core::ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) != Status::OK) {
        return Status::OK;
    }

    std::unordered_map<ID, CatalogManager::SessionInfo, IDHash> sessions_by_id;
    if (catalog_manager_) {
        std::vector<CatalogManager::SessionInfo> sessions;
        if (catalog_manager_->listSessions(sessions, nullptr) == Status::OK) {
            for (const auto& session : sessions) {
                sessions_by_id[session.session_id] = session;
            }
        }
    }

    for (const auto& backend : backends) {
        const CatalogManager::SessionInfo* session = nullptr;
        auto it = sessions_by_id.find(backend.session_id);
        if (it != sessions_by_id.end()) {
            session = &it->second;
        }

        VirtualRow row;
        row.columns = {
            {"MON$ATTACHMENT_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id + 1))},
            {"MON$SERVER_PID", backend.backend_pid == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(backend.backend_pid))},
            {"MON$STATE", TypedValue::makeInt64(1)},  // Active
            {"MON$ATTACHMENT_NAME", TypedValue::makeVarchar("scratchbird.sbdb")},
            {"MON$USER", session ? TypedValue::makeVarchar(session->username) : TypedValue::makeVarchar("SYSDBA")},
            {"MON$ROLE", TypedValue()},
            {"MON$REMOTE_PROTOCOL", TypedValue::makeVarchar("TCP/IP")},
            {"MON$REMOTE_ADDRESS", TypedValue::makeVarchar("127.0.0.1")},
            {"MON$REMOTE_PID", TypedValue()},
            {"MON$CHARACTER_SET_ID", TypedValue::makeInt64(4)},  // UTF8
            {"MON$TIMESTAMP", backend.start_time == 0 ? TypedValue() : TypedValue::makeTimestamp(static_cast<int64_t>(backend.start_time))},
            {"MON$GARBAGE_COLLECTION", TypedValue::makeInt64(1)},
            {"MON$REMOTE_PROCESS", TypedValue()},
            {"MON$STAT_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id + 1))},
            {"MON$CLIENT_VERSION", TypedValue::makeVarchar("ScratchBird 1.0")},
            {"MON$REMOTE_VERSION", TypedValue::makeVarchar("ScratchBird 1.0")}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonTransactions(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$TRANSACTION_ID", "MON$ATTACHMENT_ID", "MON$STATE", "MON$TIMESTAMP",
        "MON$TOP_TRANSACTION", "MON$OLDEST_TRANSACTION", "MON$OLDEST_ACTIVE",
        "MON$ISOLATION_MODE", "MON$LOCK_TIMEOUT", "MON$READ_ONLY",
        "MON$AUTO_COMMIT", "MON$AUTO_UNDO", "MON$STAT_ID"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::INT16, DataType::TIMESTAMP,
        DataType::INT64, DataType::INT64, DataType::INT64,
        DataType::INT16, DataType::INT64, DataType::INT16,
        DataType::INT16, DataType::INT16, DataType::INT64
    };

    std::vector<ProcessControlBlock> backends;
    core::ErrorContext proc_ctx;
    if (core::ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) != Status::OK) {
        return Status::OK;
    }

    for (const auto& backend : backends) {
        if (backend.xid == 0) {
            continue;
        }
        VirtualRow row;
        row.columns = {
            {"MON$TRANSACTION_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
            {"MON$ATTACHMENT_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id + 1))},
            {"MON$STATE", TypedValue::makeInt64(1)},  // Active
            {"MON$TIMESTAMP", backend.xact_start_time == 0 ? TypedValue() : TypedValue::makeTimestamp(static_cast<int64_t>(backend.xact_start_time))},
            {"MON$TOP_TRANSACTION", TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
            {"MON$OLDEST_TRANSACTION", TypedValue::makeInt64(1)},
            {"MON$OLDEST_ACTIVE", TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
            {"MON$ISOLATION_MODE", TypedValue::makeInt64(static_cast<int64_t>(backend.isolation_level))},
            {"MON$LOCK_TIMEOUT", TypedValue::makeInt64(-1)},  // Infinite
            {"MON$READ_ONLY", TypedValue::makeInt64(backend.is_read_only ? 1 : 0)},
            {"MON$AUTO_COMMIT", TypedValue::makeInt64(0)},
            {"MON$AUTO_UNDO", TypedValue::makeInt64(1)},
            {"MON$STAT_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id + 1))}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonStatements(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$STATEMENT_ID", "MON$ATTACHMENT_ID", "MON$TRANSACTION_ID",
        "MON$STATE", "MON$TIMESTAMP", "MON$SQL_TEXT", "MON$STAT_ID"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::INT64,
        DataType::INT16, DataType::TIMESTAMP, DataType::TEXT, DataType::INT64
    };

    std::vector<ProcessControlBlock> backends;
    core::ErrorContext proc_ctx;
    if (core::ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) != Status::OK) {
        return Status::OK;
    }

    for (const auto& backend : backends) {
        if (backend.query_start_time == 0) {
            continue;
        }

        std::string sql_text;
        if (backend.query_text[0] != '\0') {
            sql_text = backend.query_text;
        }

        VirtualRow row;
        row.columns = {
            {"MON$STATEMENT_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.query_start_time))},
            {"MON$ATTACHMENT_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id + 1))},
            {"MON$TRANSACTION_ID", backend.xid == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
            {"MON$STATE", TypedValue::makeInt64(1)},
            {"MON$TIMESTAMP", TypedValue::makeTimestamp(static_cast<int64_t>(backend.query_start_time))},
            {"MON$SQL_TEXT", sql_text.empty() ? TypedValue() : TypedValue::makeText(sql_text)},
            {"MON$STAT_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id + 1))}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonCallStack(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$STAT_ID", "MON$CALL_ID", "MON$STATEMENT_ID",
        "MON$CALL_TYPE", "MON$OBJECT_NAME"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::INT64,
        DataType::INT16, DataType::TEXT
    };
    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonIoStats(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$STAT_ID", "MON$PAGE_READS", "MON$PAGE_WRITES",
        "MON$PAGE_FETCHES", "MON$PAGE_MARKS"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::INT64,
        DataType::INT64, DataType::INT64
    };
    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonRecordStats(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$STAT_ID", "MON$RECORD_IDX_READS", "MON$RECORD_INSERTS",
        "MON$RECORD_UPDATES", "MON$RECORD_DELETES"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::INT64,
        DataType::INT64, DataType::INT64
    };
    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonMemoryUsage(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$STAT_ID", "MON$MEMORY_USED", "MON$MEMORY_ALLOCATED"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::INT64
    };
    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonTableStats(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$STAT_ID", "MON$TABLE_NAME"
    };
    results.column_types = {
        DataType::INT64, DataType::TEXT
    };
    return Status::OK;
}

Status FirebirdCatalogHandler::queryMonContextVariables(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "MON$ATTACHMENT_ID", "MON$TRANSACTION_ID", "MON$VARIABLE_NAME", "MON$VARIABLE_VALUE"
    };
    results.column_types = {
        DataType::INT64, DataType::INT64, DataType::TEXT, DataType::TEXT
    };
    return Status::OK;
}

// ============================================================================
// SEC$ Table Queries
// ============================================================================

Status FirebirdCatalogHandler::querySecUsers(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "SEC$USER_NAME", "SEC$FIRST_NAME", "SEC$MIDDLE_NAME", "SEC$LAST_NAME",
        "SEC$ACTIVE", "SEC$ADMIN", "SEC$DESCRIPTION", "SEC$PLUGIN"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT,
        DataType::BOOLEAN, DataType::BOOLEAN, DataType::TEXT, DataType::TEXT
    };

    // Add default SYSDBA user
    VirtualRow row;
    row.columns = {
        {"SEC$USER_NAME", TypedValue::makeVarchar("SYSDBA")},
        {"SEC$FIRST_NAME", TypedValue::makeVarchar("System")},
        {"SEC$MIDDLE_NAME", TypedValue()},  // NULL
        {"SEC$LAST_NAME", TypedValue::makeVarchar("Administrator")},
        {"SEC$ACTIVE", TypedValue::makeBool(true)},
        {"SEC$ADMIN", TypedValue::makeBool(true)},
        {"SEC$DESCRIPTION", TypedValue::makeVarchar("Database administrator")},
        {"SEC$PLUGIN", TypedValue::makeVarchar("Srp256")}
    };
    results.rows.push_back(row);

    return Status::OK;
}

Status FirebirdCatalogHandler::querySecUserAttributes(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "SEC$USER_NAME", "SEC$ATTR_NAME", "SEC$ATTR_VALUE", "SEC$PLUGIN"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT
    };

    // No extended attributes by default
    return Status::OK;
}

Status FirebirdCatalogHandler::querySecDbCreators(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "SEC$USER_NAME"
    };
    results.column_types = {
        DataType::TEXT
    };

    // Default SYSDBA creator for compatibility
    VirtualRow row;
    row.columns = {
        {"SEC$USER_NAME", TypedValue::makeVarchar("SYSDBA")}
    };
    results.rows.push_back(row);

    return Status::OK;
}

Status FirebirdCatalogHandler::querySecGlobalAuthMapping(VirtualResultSet& results, ErrorContext* /* ctx */) {
    results.column_names = {
        "SEC$MAP_NAME", "SEC$MAP_USING", "SEC$MAP_PLUGIN", "SEC$MAP_FROM", "SEC$MAP_TO"
    };
    results.column_types = {
        DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT
    };

    return Status::OK;
}

// ============================================================================
// Column Definitions
// ============================================================================

// Helper to create a ColumnInfo with minimal fields
static CatalogManager::ColumnInfo makeCol(const std::string& name, DataType type, bool nullable) {
    CatalogManager::ColumnInfo col;
    col.column_name = name;
    col.data_type = static_cast<uint16_t>(type);
    col.nullable = nullable;
    return col;
}

void FirebirdCatalogHandler::getRdbDatabaseColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$RELATION_ID", DataType::INT64, true));
    cols.push_back(makeCol("RDB$SECURITY_CLASS", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$CHARACTER_SET_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$LINGER", DataType::INT64, true));
    cols.push_back(makeCol("RDB$SQL_SECURITY", DataType::BOOLEAN, true));
}

void FirebirdCatalogHandler::getRdbRelationsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$RELATION_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
    cols.push_back(makeCol("RDB$RELATION_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$OWNER_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$VIEW_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$EXTERNAL_FILE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$RELATION_ID", DataType::INT64, true));
}

void FirebirdCatalogHandler::getRdbFieldsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$FIELD_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$FIELD_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$FIELD_LENGTH", DataType::INT16, true));
    cols.push_back(makeCol("RDB$FIELD_SCALE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$CHARACTER_SET_ID", DataType::INT16, true));
    cols.push_back(makeCol("RDB$COLLATION_ID", DataType::INT16, true));
    cols.push_back(makeCol("RDB$NULL_FLAG", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DEFAULT_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getRdbRelationFieldsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$RELATION_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$FIELD_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$FIELD_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$FIELD_POSITION", DataType::INT16, true));
    cols.push_back(makeCol("RDB$NULL_FLAG", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DEFAULT_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbIndicesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$INDEX_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$RELATION_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$UNIQUE_FLAG", DataType::INT16, true));
    cols.push_back(makeCol("RDB$INDEX_INACTIVE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$INDEX_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$SEGMENT_COUNT", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbIndexSegmentsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$INDEX_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$FIELD_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$FIELD_POSITION", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbGeneratorsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$GENERATOR_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$GENERATOR_ID", DataType::INT64, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$OWNER_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$INITIAL_VALUE", DataType::INT64, true));
    cols.push_back(makeCol("RDB$GENERATOR_INCREMENT", DataType::INT64, true));
}

void FirebirdCatalogHandler::getRdbProceduresColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$PROCEDURE_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$PROCEDURE_ID", DataType::INT64, true));
    cols.push_back(makeCol("RDB$PROCEDURE_INPUTS", DataType::INT16, true));
    cols.push_back(makeCol("RDB$PROCEDURE_OUTPUTS", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$PROCEDURE_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$OWNER_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbProcedureParametersColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$PROCEDURE_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$PARAMETER_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$PARAMETER_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$FIELD_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DEFAULT_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$PARAMETER_NUMBER", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbFunctionsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$FUNCTION_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$FUNCTION_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$RETURN_ARGUMENT", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$MODULE_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$ENTRYPOINT", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$OWNER_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbFunctionArgumentsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$FUNCTION_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$ARGUMENT_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$ARGUMENT_POSITION", DataType::INT16, true));
    cols.push_back(makeCol("RDB$MECHANISM", DataType::INT16, true));
    cols.push_back(makeCol("RDB$FIELD_SOURCE", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getRdbTriggersColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$TRIGGER_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$RELATION_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$TRIGGER_SEQUENCE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$TRIGGER_TYPE", DataType::INT64, true));
    cols.push_back(makeCol("RDB$TRIGGER_SOURCE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$TRIGGER_INACTIVE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$CONSTRAINT_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$CONSTRAINT_TYPE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$RELATION_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$INDEX_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DEFERRABLE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$INITIALLY_DEFERRED", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getRdbRelationConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    getRdbConstraintsColumns(cols);
}

void FirebirdCatalogHandler::getRdbViewRelationsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$VIEW_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$RELATION_NAME", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getRdbCheckConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$CONSTRAINT_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$TRIGGER_NAME", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getRdbRefConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$CONSTRAINT_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$CONST_NAME_UQ", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$MATCH_OPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$UPDATE_RULE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DELETE_RULE", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getRdbExceptionsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$EXCEPTION_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$MESSAGE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbUserPrivilegesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$USER", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$GRANTOR", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$PRIVILEGE", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$GRANT_OPTION", DataType::INT16, true));
    cols.push_back(makeCol("RDB$RELATION_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$FIELD_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$USER_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$OBJECT_TYPE", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbRolesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$ROLE_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$OWNER_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbDependenciesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$DEPENDENT_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DEPENDENT_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DEPENDED_ON_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DEPENDED_ON_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$FIELD_NAME", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getRdbPackagesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$PACKAGE_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$OWNER_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbKeywordsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$KEYWORD_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$KEYWORD_TYPE", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbCharacterSetsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$CHARACTER_SET_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$CHARACTER_SET_ID", DataType::INT16, true));
    cols.push_back(makeCol("RDB$BYTES_PER_CHARACTER", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DEFAULT_COLLATE_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbCollationsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$COLLATION_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$COLLATION_ID", DataType::INT16, true));
    cols.push_back(makeCol("RDB$CHARACTER_SET_ID", DataType::INT16, true));
    cols.push_back(makeCol("RDB$COLLATION_ATTRIBUTES", DataType::INT16, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getRdbTypesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("RDB$TYPE_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("RDB$TYPE", DataType::INT16, true));
    cols.push_back(makeCol("RDB$FIELD_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("RDB$SYSTEM_FLAG", DataType::INT16, true));
}

void FirebirdCatalogHandler::getMonDatabaseColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$DATABASE_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("MON$PAGE_SIZE", DataType::INT64, true));
    cols.push_back(makeCol("MON$ODS_MAJOR", DataType::INT16, true));
    cols.push_back(makeCol("MON$ODS_MINOR", DataType::INT16, true));
    cols.push_back(makeCol("MON$OLDEST_TRANSACTION", DataType::INT64, true));
    cols.push_back(makeCol("MON$OLDEST_ACTIVE", DataType::INT64, true));
    cols.push_back(makeCol("MON$OLDEST_SNAPSHOT", DataType::INT64, true));
    cols.push_back(makeCol("MON$NEXT_TRANSACTION", DataType::INT64, true));
    cols.push_back(makeCol("MON$PAGE_BUFFERS", DataType::INT64, true));
    cols.push_back(makeCol("MON$SQL_DIALECT", DataType::INT16, true));
    cols.push_back(makeCol("MON$SHUTDOWN_MODE", DataType::INT16, true));
    cols.push_back(makeCol("MON$SWEEP_INTERVAL", DataType::INT64, true));
    cols.push_back(makeCol("MON$READ_ONLY", DataType::INT16, true));
    cols.push_back(makeCol("MON$FORCED_WRITES", DataType::INT16, true));
    cols.push_back(makeCol("MON$RESERVE_SPACE", DataType::INT16, true));
    cols.push_back(makeCol("MON$CREATION_DATE", DataType::TIMESTAMP, true));
    cols.push_back(makeCol("MON$PAGES", DataType::INT64, true));
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$BACKUP_STATE", DataType::INT16, true));
    cols.push_back(makeCol("MON$CRYPT_PAGE", DataType::INT64, true));
    cols.push_back(makeCol("MON$OWNER", DataType::TEXT, true));
    cols.push_back(makeCol("MON$SEC_DATABASE", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getMonAttachmentsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$ATTACHMENT_ID", DataType::INT64, false));
    cols.push_back(makeCol("MON$SERVER_PID", DataType::INT64, true));
    cols.push_back(makeCol("MON$STATE", DataType::INT16, true));
    cols.push_back(makeCol("MON$ATTACHMENT_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("MON$USER", DataType::TEXT, true));
    cols.push_back(makeCol("MON$ROLE", DataType::TEXT, true));
    cols.push_back(makeCol("MON$REMOTE_PROTOCOL", DataType::TEXT, true));
    cols.push_back(makeCol("MON$REMOTE_ADDRESS", DataType::TEXT, true));
    cols.push_back(makeCol("MON$REMOTE_PID", DataType::INT64, true));
    cols.push_back(makeCol("MON$CHARACTER_SET_ID", DataType::INT16, true));
    cols.push_back(makeCol("MON$TIMESTAMP", DataType::TIMESTAMP, true));
    cols.push_back(makeCol("MON$GARBAGE_COLLECTION", DataType::INT16, true));
    cols.push_back(makeCol("MON$REMOTE_PROCESS", DataType::TEXT, true));
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$CLIENT_VERSION", DataType::TEXT, true));
    cols.push_back(makeCol("MON$REMOTE_VERSION", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getMonTransactionsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$TRANSACTION_ID", DataType::INT64, false));
    cols.push_back(makeCol("MON$ATTACHMENT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$STATE", DataType::INT16, true));
    cols.push_back(makeCol("MON$TIMESTAMP", DataType::TIMESTAMP, true));
    cols.push_back(makeCol("MON$TOP_TRANSACTION", DataType::INT64, true));
    cols.push_back(makeCol("MON$OLDEST_TRANSACTION", DataType::INT64, true));
    cols.push_back(makeCol("MON$OLDEST_ACTIVE", DataType::INT64, true));
    cols.push_back(makeCol("MON$ISOLATION_MODE", DataType::INT16, true));
    cols.push_back(makeCol("MON$LOCK_TIMEOUT", DataType::INT64, true));
    cols.push_back(makeCol("MON$READ_ONLY", DataType::INT16, true));
    cols.push_back(makeCol("MON$AUTO_COMMIT", DataType::INT16, true));
    cols.push_back(makeCol("MON$AUTO_UNDO", DataType::INT16, true));
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
}

void FirebirdCatalogHandler::getMonStatementsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$STATEMENT_ID", DataType::INT64, false));
    cols.push_back(makeCol("MON$ATTACHMENT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$TRANSACTION_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$STATE", DataType::INT16, true));
    cols.push_back(makeCol("MON$TIMESTAMP", DataType::TIMESTAMP, true));
    cols.push_back(makeCol("MON$SQL_TEXT", DataType::TEXT, true));
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
}

void FirebirdCatalogHandler::getMonCallStackColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$CALL_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$STATEMENT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$CALL_TYPE", DataType::INT16, true));
    cols.push_back(makeCol("MON$OBJECT_NAME", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getMonIoStatsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$PAGE_READS", DataType::INT64, true));
    cols.push_back(makeCol("MON$PAGE_WRITES", DataType::INT64, true));
    cols.push_back(makeCol("MON$PAGE_FETCHES", DataType::INT64, true));
    cols.push_back(makeCol("MON$PAGE_MARKS", DataType::INT64, true));
}

void FirebirdCatalogHandler::getMonRecordStatsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$RECORD_IDX_READS", DataType::INT64, true));
    cols.push_back(makeCol("MON$RECORD_INSERTS", DataType::INT64, true));
    cols.push_back(makeCol("MON$RECORD_UPDATES", DataType::INT64, true));
    cols.push_back(makeCol("MON$RECORD_DELETES", DataType::INT64, true));
}

void FirebirdCatalogHandler::getMonMemoryUsageColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$MEMORY_USED", DataType::INT64, true));
    cols.push_back(makeCol("MON$MEMORY_ALLOCATED", DataType::INT64, true));
}

void FirebirdCatalogHandler::getMonTableStatsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$STAT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$TABLE_NAME", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getMonContextVariablesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("MON$ATTACHMENT_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$TRANSACTION_ID", DataType::INT64, true));
    cols.push_back(makeCol("MON$VARIABLE_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("MON$VARIABLE_VALUE", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getSecUsersColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("SEC$USER_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("SEC$FIRST_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$MIDDLE_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$LAST_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$ACTIVE", DataType::BOOLEAN, true));
    cols.push_back(makeCol("SEC$ADMIN", DataType::BOOLEAN, true));
    cols.push_back(makeCol("SEC$DESCRIPTION", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$PLUGIN", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getSecUserAttributesColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("SEC$USER_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("SEC$ATTR_NAME", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$ATTR_VALUE", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$PLUGIN", DataType::TEXT, true));
}

void FirebirdCatalogHandler::getSecDbCreatorsColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("SEC$USER_NAME", DataType::TEXT, false));
}

void FirebirdCatalogHandler::getSecGlobalAuthMappingColumns(std::vector<CatalogManager::ColumnInfo>& cols) {
    cols.clear();
    cols.push_back(makeCol("SEC$MAP_NAME", DataType::TEXT, false));
    cols.push_back(makeCol("SEC$MAP_USING", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$MAP_PLUGIN", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$MAP_FROM", DataType::TEXT, true));
    cols.push_back(makeCol("SEC$MAP_TO", DataType::TEXT, true));
}

} // namespace scratchbird::catalog
