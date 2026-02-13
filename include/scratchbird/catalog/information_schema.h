/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * SQL Standard information_schema Implementation
 *
 * Phase D: Catalog Cleanup - information_schema virtual catalog handler
 *
 * Created: November 26, 2025
 * Phase: Catalog Cleanup Phase D
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * InformationSchemaHandler - SQL standard information_schema implementation
 */
class InformationSchemaHandler : public VirtualCatalogHandler {
public:
    explicit InformationSchemaHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    ProtocolType getProtocolType() const override {
        return ProtocolType::SCRATCHBIRD;
    }

    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "information_schema");
    }

    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override {
        if (!ownsSchema(schema_name)) {
            return false;
        }
        for (const auto& name : table_names_) {
            if (equalsCaseInsensitive(table_name, name)) {
                return true;
            }
        }
        return false;
    }

    Status queryTable(const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& /* where_clause */,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Schema not found: " + schema_name).c_str());
            return Status::NOT_FOUND;
        }

        if (!ownsTable(schema_name, table_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table not found: information_schema." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        const ColumnDefs* def = getTableDefinition(table_name);
        if (!def) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table definition not found: information_schema." + table_name).c_str());
            return Status::NOT_FOUND;
        }
        setResultColumns(*def, results);

        if (equalsCaseInsensitive(table_name, "schemata")) {
            return querySchemata(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "tables")) {
            return queryTables(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "columns")) {
            return queryColumns(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "views")) {
            return queryViews(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "routines")) {
            return queryRoutines(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "parameters")) {
            return queryParameters(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "triggers")) {
            return queryTriggers(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "sequences")) {
            return querySequences(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "table_constraints")) {
            return queryTableConstraints(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "key_column_usage")) {
            return queryKeyColumnUsage(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "referential_constraints")) {
            return queryReferentialConstraints(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "check_constraints")) {
            return queryCheckConstraints(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "statistics")) {
            return queryStatistics(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "domains")) {
            return queryDomains(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "user_defined_types")) {
            return queryUserDefinedTypes(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "table_privileges")) {
            return queryTablePrivileges(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "schema_privileges")) {
            return querySchemaPrivileges(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "database_privileges")) {
            return queryDatabasePrivileges(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "usage_privileges")) {
            return queryUsagePrivileges(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "routine_privileges")) {
            return queryRoutinePrivileges(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "column_privileges")) {
            return queryColumnPrivileges(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "processlist")) {
            return queryProcesslist(results, ctx);
        }

        return Status::OK;
    }

    Status getTableColumns(const std::string& schema_name,
                           const std::string& table_name,
                           std::vector<CatalogManager::ColumnInfo>& columns,
                           ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Schema not found: " + schema_name).c_str());
            return Status::NOT_FOUND;
        }

        if (!ownsTable(schema_name, table_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table not found: information_schema." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        const ColumnDefs* def = getTableDefinition(table_name);
        if (!def) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table definition not found: information_schema." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        setColumnInfo(*def, columns);
        return Status::OK;
    }

    Status listTables(const std::string& schema_name,
                      std::vector<std::string>& table_names,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Schema not found: " + schema_name).c_str());
            return Status::NOT_FOUND;
        }

        table_names = table_names_;
        return Status::OK;
    }

    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* /* ctx */ = nullptr) override {
        schema_names.clear();
        schema_names.push_back("information_schema");
        return Status::OK;
    }

private:
    struct ColumnDef {
        const char* name;
        DataType type;
        bool nullable;
    };

    using ColumnDefs = std::vector<ColumnDef>;

    std::vector<std::string> table_names_;

    static const char* defaultCatalogName() { return "def"; }

    void initializeTableNames() {
        table_names_ = {
            "SCHEMATA",
            "TABLES",
            "COLUMNS",
            "TABLE_CONSTRAINTS",
            "KEY_COLUMN_USAGE",
            "REFERENTIAL_CONSTRAINTS",
            "CHECK_CONSTRAINTS",
            "VIEWS",
            "ROUTINES",
            "PARAMETERS",
            "TRIGGERS",
            "SEQUENCES",
            "DOMAINS",
            "USER_DEFINED_TYPES",
            "STATISTICS",
            "TABLE_PRIVILEGES",
            "SCHEMA_PRIVILEGES",
            "DATABASE_PRIVILEGES",
            "USAGE_PRIVILEGES",
            "ROUTINE_PRIVILEGES",
            "COLUMN_PRIVILEGES",
            "PROCESSLIST"
        };
    }

    const ColumnDefs* getTableDefinition(const std::string& table_name) const {
        if (equalsCaseInsensitive(table_name, "schemata")) {
            static const ColumnDefs cols = {
                {"CATALOG_NAME", DataType::VARCHAR, true},
                {"SCHEMA_NAME", DataType::VARCHAR, false},
                {"SCHEMA_OWNER", DataType::VARCHAR, true},
                {"DEFAULT_CHARACTER_SET_NAME", DataType::VARCHAR, true},
                {"DEFAULT_COLLATION_NAME", DataType::VARCHAR, true},
                {"SQL_PATH", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "tables")) {
            static const ColumnDefs cols = {
                {"TABLE_CATALOG", DataType::VARCHAR, true},
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"TABLE_TYPE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "columns")) {
            static const ColumnDefs cols = {
                {"TABLE_CATALOG", DataType::VARCHAR, true},
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"COLUMN_NAME", DataType::VARCHAR, true},
                {"ORDINAL_POSITION", DataType::INT64, true},
                {"COLUMN_DEFAULT", DataType::VARCHAR, true},
                {"IS_NULLABLE", DataType::VARCHAR, true},
                {"DATA_TYPE", DataType::VARCHAR, true},
                {"CHARACTER_MAXIMUM_LENGTH", DataType::INT64, true},
                {"NUMERIC_PRECISION", DataType::INT64, true},
                {"NUMERIC_SCALE", DataType::INT64, true},
                {"IS_IDENTITY", DataType::VARCHAR, true},
                {"IDENTITY_GENERATION", DataType::VARCHAR, true},
                {"IDENTITY_START", DataType::INT64, true},
                {"IDENTITY_INCREMENT", DataType::INT64, true},
                {"IDENTITY_MAXIMUM", DataType::INT64, true},
                {"IDENTITY_MINIMUM", DataType::INT64, true},
                {"IDENTITY_CYCLE", DataType::VARCHAR, true},
                {"IS_GENERATED", DataType::VARCHAR, true},
                {"GENERATION_EXPRESSION", DataType::VARCHAR, true},
                {"DOMAIN_NAME", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "views")) {
            static const ColumnDefs cols = {
                {"TABLE_CATALOG", DataType::VARCHAR, true},
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"VIEW_DEFINITION", DataType::TEXT, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "routines")) {
            static const ColumnDefs cols = {
                {"ROUTINE_CATALOG", DataType::VARCHAR, true},
                {"ROUTINE_SCHEMA", DataType::VARCHAR, true},
                {"ROUTINE_NAME", DataType::VARCHAR, true},
                {"ROUTINE_TYPE", DataType::VARCHAR, true},
                {"DATA_TYPE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "parameters")) {
            static const ColumnDefs cols = {
                {"SPECIFIC_CATALOG", DataType::VARCHAR, true},
                {"SPECIFIC_SCHEMA", DataType::VARCHAR, true},
                {"SPECIFIC_NAME", DataType::VARCHAR, true},
                {"ORDINAL_POSITION", DataType::INT64, true},
                {"PARAMETER_MODE", DataType::VARCHAR, true},
                {"PARAMETER_NAME", DataType::VARCHAR, true},
                {"DATA_TYPE", DataType::VARCHAR, true},
                {"CHARACTER_MAXIMUM_LENGTH", DataType::INT64, true},
                {"NUMERIC_PRECISION", DataType::INT64, true},
                {"NUMERIC_SCALE", DataType::INT64, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "triggers")) {
            static const ColumnDefs cols = {
                {"TRIGGER_CATALOG", DataType::VARCHAR, true},
                {"TRIGGER_SCHEMA", DataType::VARCHAR, true},
                {"TRIGGER_NAME", DataType::VARCHAR, true},
                {"EVENT_MANIPULATION", DataType::VARCHAR, true},
                {"EVENT_OBJECT_CATALOG", DataType::VARCHAR, true},
                {"EVENT_OBJECT_SCHEMA", DataType::VARCHAR, true},
                {"EVENT_OBJECT_TABLE", DataType::VARCHAR, true},
                {"ACTION_TIMING", DataType::VARCHAR, true},
                {"ACTION_STATEMENT", DataType::TEXT, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "sequences")) {
            static const ColumnDefs cols = {
                {"SEQUENCE_CATALOG", DataType::VARCHAR, true},
                {"SEQUENCE_SCHEMA", DataType::VARCHAR, true},
                {"SEQUENCE_NAME", DataType::VARCHAR, true},
                {"DATA_TYPE", DataType::VARCHAR, true},
                {"NUMERIC_PRECISION", DataType::INT64, true},
                {"NUMERIC_SCALE", DataType::INT64, true},
                {"START_VALUE", DataType::INT64, true},
                {"MINIMUM_VALUE", DataType::INT64, true},
                {"MAXIMUM_VALUE", DataType::INT64, true},
                {"INCREMENT", DataType::INT64, true},
                {"CYCLE_OPTION", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "table_constraints")) {
            static const ColumnDefs cols = {
                {"CONSTRAINT_CATALOG", DataType::VARCHAR, true},
                {"CONSTRAINT_SCHEMA", DataType::VARCHAR, true},
                {"CONSTRAINT_NAME", DataType::VARCHAR, true},
                {"TABLE_CATALOG", DataType::VARCHAR, true},
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"CONSTRAINT_TYPE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "key_column_usage")) {
            static const ColumnDefs cols = {
                {"CONSTRAINT_CATALOG", DataType::VARCHAR, true},
                {"CONSTRAINT_SCHEMA", DataType::VARCHAR, true},
                {"CONSTRAINT_NAME", DataType::VARCHAR, true},
                {"TABLE_CATALOG", DataType::VARCHAR, true},
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"COLUMN_NAME", DataType::VARCHAR, true},
                {"ORDINAL_POSITION", DataType::INT64, true},
                {"POSITION_IN_UNIQUE_CONSTRAINT", DataType::INT64, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "referential_constraints")) {
            static const ColumnDefs cols = {
                {"CONSTRAINT_CATALOG", DataType::VARCHAR, true},
                {"CONSTRAINT_SCHEMA", DataType::VARCHAR, true},
                {"CONSTRAINT_NAME", DataType::VARCHAR, true},
                {"UNIQUE_CONSTRAINT_CATALOG", DataType::VARCHAR, true},
                {"UNIQUE_CONSTRAINT_SCHEMA", DataType::VARCHAR, true},
                {"UNIQUE_CONSTRAINT_NAME", DataType::VARCHAR, true},
                {"MATCH_OPTION", DataType::VARCHAR, true},
                {"UPDATE_RULE", DataType::VARCHAR, true},
                {"DELETE_RULE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "check_constraints")) {
            static const ColumnDefs cols = {
                {"CONSTRAINT_CATALOG", DataType::VARCHAR, true},
                {"CONSTRAINT_SCHEMA", DataType::VARCHAR, true},
                {"CONSTRAINT_NAME", DataType::VARCHAR, true},
                {"CHECK_CLAUSE", DataType::TEXT, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "statistics")) {
            static const ColumnDefs cols = {
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"NON_UNIQUE", DataType::INT64, true},
                {"INDEX_SCHEMA", DataType::VARCHAR, true},
                {"INDEX_NAME", DataType::VARCHAR, true},
                {"SEQ_IN_INDEX", DataType::INT64, true},
                {"COLUMN_NAME", DataType::VARCHAR, true},
                {"COLLATION", DataType::VARCHAR, true},
                {"CARDINALITY", DataType::INT64, true},
                {"INDEX_TYPE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "domains")) {
            static const ColumnDefs cols = {
                {"DOMAIN_CATALOG", DataType::VARCHAR, true},
                {"DOMAIN_SCHEMA", DataType::VARCHAR, true},
                {"DOMAIN_NAME", DataType::VARCHAR, true},
                {"DATA_TYPE", DataType::VARCHAR, true},
                {"CHARACTER_MAXIMUM_LENGTH", DataType::INT64, true},
                {"NUMERIC_PRECISION", DataType::INT64, true},
                {"NUMERIC_SCALE", DataType::INT64, true},
                {"DOMAIN_DEFAULT", DataType::VARCHAR, true},
                {"IS_NULLABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "user_defined_types")) {
            static const ColumnDefs cols = {
                {"USER_DEFINED_TYPE_CATALOG", DataType::VARCHAR, true},
                {"USER_DEFINED_TYPE_SCHEMA", DataType::VARCHAR, true},
                {"USER_DEFINED_TYPE_NAME", DataType::VARCHAR, true},
                {"DATA_TYPE", DataType::VARCHAR, true},
                {"IS_ORDERABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "table_privileges")) {
            static const ColumnDefs cols = {
                {"GRANTOR", DataType::VARCHAR, true},
                {"GRANTEE", DataType::VARCHAR, true},
                {"TABLE_CATALOG", DataType::VARCHAR, true},
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"PRIVILEGE_TYPE", DataType::VARCHAR, true},
                {"IS_GRANTABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "schema_privileges")) {
            static const ColumnDefs cols = {
                {"GRANTOR", DataType::VARCHAR, true},
                {"GRANTEE", DataType::VARCHAR, true},
                {"SCHEMA_NAME", DataType::VARCHAR, true},
                {"PRIVILEGE_TYPE", DataType::VARCHAR, true},
                {"IS_GRANTABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "database_privileges")) {
            static const ColumnDefs cols = {
                {"GRANTOR", DataType::VARCHAR, true},
                {"GRANTEE", DataType::VARCHAR, true},
                {"DATABASE_NAME", DataType::VARCHAR, true},
                {"PRIVILEGE_TYPE", DataType::VARCHAR, true},
                {"IS_GRANTABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "usage_privileges")) {
            static const ColumnDefs cols = {
                {"GRANTOR", DataType::VARCHAR, true},
                {"GRANTEE", DataType::VARCHAR, true},
                {"OBJECT_SCHEMA", DataType::VARCHAR, true},
                {"OBJECT_NAME", DataType::VARCHAR, true},
                {"PRIVILEGE_TYPE", DataType::VARCHAR, true},
                {"IS_GRANTABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "routine_privileges")) {
            static const ColumnDefs cols = {
                {"GRANTOR", DataType::VARCHAR, true},
                {"GRANTEE", DataType::VARCHAR, true},
                {"ROUTINE_SCHEMA", DataType::VARCHAR, true},
                {"ROUTINE_NAME", DataType::VARCHAR, true},
                {"PRIVILEGE_TYPE", DataType::VARCHAR, true},
                {"IS_GRANTABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "column_privileges")) {
            static const ColumnDefs cols = {
                {"GRANTOR", DataType::VARCHAR, true},
                {"GRANTEE", DataType::VARCHAR, true},
                {"TABLE_SCHEMA", DataType::VARCHAR, true},
                {"TABLE_NAME", DataType::VARCHAR, true},
                {"COLUMN_NAME", DataType::VARCHAR, true},
                {"PRIVILEGE_TYPE", DataType::VARCHAR, true},
                {"IS_GRANTABLE", DataType::VARCHAR, true}
            };
            return &cols;
        }
        if (equalsCaseInsensitive(table_name, "processlist")) {
            static const ColumnDefs cols = {
                {"ID", DataType::INT64, false},
                {"USER", DataType::VARCHAR, true},
                {"HOST", DataType::VARCHAR, true},
                {"DB", DataType::VARCHAR, true},
                {"COMMAND", DataType::VARCHAR, true},
                {"TIME", DataType::INT64, true},
                {"STATE", DataType::VARCHAR, true},
                {"INFO", DataType::TEXT, true}
            };
            return &cols;
        }
        return nullptr;
    }

    void setResultColumns(const ColumnDefs& cols, VirtualResultSet& results) const {
        results.column_names.clear();
        results.column_types.clear();
        results.rows.clear();
        for (const auto& col : cols) {
            results.column_names.emplace_back(col.name);
            results.column_types.push_back(col.type);
        }
    }

    void setColumnInfo(const ColumnDefs& cols,
                       std::vector<CatalogManager::ColumnInfo>& columns) const {
        columns.clear();
        uint16_t ordinal = 1;
        for (const auto& col : cols) {
            CatalogManager::ColumnInfo info;
            info.column_name = col.name;
            info.data_type = static_cast<uint16_t>(col.type);
            info.nullable = col.nullable;
            info.ordinal = ordinal++;
            columns.push_back(std::move(info));
        }
    }

    static std::string yesNo(bool value) {
        return value ? "YES" : "NO";
    }

    static std::string dataTypeName(DataType type) {
        return TypeSystem::getTypeName(type);
    }

    static std::string domainTypeName(DomainType type) {
        switch (type) {
            case DomainType::BASIC: return "DOMAIN";
            case DomainType::RECORD: return "ROW";
            case DomainType::ENUM: return "ENUM";
            case DomainType::SET: return "SET";
            case DomainType::VARIANT: return "VARIANT";
            default: return "DOMAIN";
        }
    }

    static std::string constraintTypeName(CatalogManager::ConstraintType type) {
        switch (type) {
            case CatalogManager::ConstraintType::PRIMARY_KEY: return "PRIMARY KEY";
            case CatalogManager::ConstraintType::UNIQUE: return "UNIQUE";
            case CatalogManager::ConstraintType::CHECK: return "CHECK";
            case CatalogManager::ConstraintType::FOREIGN_KEY: return "FOREIGN KEY";
            case CatalogManager::ConstraintType::NOT_NULL: return "NOT NULL";
            case CatalogManager::ConstraintType::EXCLUSION: return "EXCLUSION";
            default: return "UNKNOWN";
        }
    }

    static std::string indexTypeName(CatalogManager::IndexType type) {
        switch (type) {
            case CatalogManager::IndexType::BTREE: return "BTREE";
            case CatalogManager::IndexType::HASH: return "HASH";
            case CatalogManager::IndexType::HNSW: return "HNSW";
            case CatalogManager::IndexType::FULLTEXT: return "FULLTEXT";
            case CatalogManager::IndexType::GIN: return "GIN";
            case CatalogManager::IndexType::GIST: return "GIST";
            case CatalogManager::IndexType::BRIN: return "BRIN";
            case CatalogManager::IndexType::RTREE: return "RTREE";
            case CatalogManager::IndexType::SPGIST: return "SPGIST";
            case CatalogManager::IndexType::BITMAP: return "BITMAP";
            case CatalogManager::IndexType::COLUMNSTORE: return "COLUMNSTORE";
            case CatalogManager::IndexType::LSM: return "LSM";
            default: return "UNKNOWN";
        }
    }

    static std::string fkActionName(CatalogManager::FKAction action) {
        switch (action) {
            case CatalogManager::FKAction::CASCADE: return "CASCADE";
            case CatalogManager::FKAction::SET_NULL: return "SET NULL";
            case CatalogManager::FKAction::SET_DEFAULT: return "SET DEFAULT";
            case CatalogManager::FKAction::RESTRICT: return "RESTRICT";
            case CatalogManager::FKAction::NO_ACTION: return "NO ACTION";
            default: return "NO ACTION";
        }
    }

    static std::string fkMatchName(CatalogManager::FKMatchType match) {
        switch (match) {
            case CatalogManager::FKMatchType::SIMPLE: return "SIMPLE";
            case CatalogManager::FKMatchType::FULL: return "FULL";
            case CatalogManager::FKMatchType::PARTIAL: return "PARTIAL";
            default: return "SIMPLE";
        }
    }

    bool getSchemaPath(const ID& schema_id, std::string& schema_out) const {
        if (!catalog_manager_) {
            return false;
        }
        CatalogManager::SchemaInfo schema_info;
        if (catalog_manager_->getSchema(schema_id, schema_info) != Status::OK) {
            return false;
        }
        schema_out = schema_info.full_path.empty() ? schema_info.schema_name : schema_info.full_path;
        return true;
    }

    std::string resolveUserName(const ID& user_id) const {
        if (!catalog_manager_) {
            return std::string();
        }
        CatalogManager::UserInfo user;
        ErrorContext ctx;
        if (catalog_manager_->getUser(user_id, user, &ctx) == Status::OK) {
            return user.username;
        }
        return user_id.toString();
    }

    std::string resolveRoleName(const ID& role_id) const {
        if (!catalog_manager_) {
            return std::string();
        }
        CatalogManager::RoleInfo role;
        ErrorContext ctx;
        if (catalog_manager_->getRole(role_id, role, &ctx) == Status::OK) {
            return role.role_name;
        }
        return role_id.toString();
    }

    std::string resolveGroupName(const ID& group_id) const {
        if (!catalog_manager_) {
            return std::string();
        }
        CatalogManager::GroupInfo group;
        ErrorContext ctx;
        if (catalog_manager_->getGroup(group_id, group, &ctx) == Status::OK) {
            return group.group_name;
        }
        return group_id.toString();
    }

    std::string resolveGranteeName(CatalogManager::GranteeType type, const ID& id) const {
        switch (type) {
            case CatalogManager::GranteeType::PUBLIC:
                return "PUBLIC";
            case CatalogManager::GranteeType::USER:
                return resolveUserName(id);
            case CatalogManager::GranteeType::ROLE:
                return resolveRoleName(id);
            case CatalogManager::GranteeType::GROUP:
                return resolveGroupName(id);
            default:
                return id.toString();
        }
    }

    std::string resolveGrantorName(const ID& id) const {
        return resolveUserName(id);
    }

    bool numericPrecision(DataType type, uint32_t precision, int64_t& out) const {
        if (precision > 0) {
            out = static_cast<int64_t>(precision);
            return true;
        }
        switch (type) {
            case DataType::INT8: out = 8; return true;
            case DataType::INT16: out = 16; return true;
            case DataType::INT32: out = 32; return true;
            case DataType::INT64: out = 64; return true;
            case DataType::UINT8: out = 8; return true;
            case DataType::UINT16: out = 16; return true;
            case DataType::UINT32: out = 32; return true;
            case DataType::UINT64: out = 64; return true;
            case DataType::FLOAT32: out = 24; return true;
            case DataType::FLOAT64: out = 53; return true;
            default:
                return false;
        }
    }

    bool numericScale(DataType type, uint32_t scale, int64_t& out) const {
        if (type == DataType::DECIMAL) {
            out = static_cast<int64_t>(scale);
            return true;
        }
        return false;
    }

    bool characterMaxLength(DataType type, uint32_t precision, uint32_t max_length,
                            int64_t& out) const {
        if (type == DataType::VARCHAR || type == DataType::CHAR) {
            if (precision > 0) {
                out = static_cast<int64_t>(precision);
            } else if (max_length > 0) {
                out = static_cast<int64_t>(max_length);
            } else {
                out = 0;
            }
            return true;
        }
        return false;
    }

    Status querySchemata(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            VirtualRow row;
            row.columns = {
                {"CATALOG_NAME", TypedValue::makeVarchar(defaultCatalogName())},
                {"SCHEMA_NAME", TypedValue::makeVarchar(schema_name)},
                {"SCHEMA_OWNER", TypedValue::makeVarchar(resolveUserName(schema.owner_id))},
                {"DEFAULT_CHARACTER_SET_NAME", TypedValue::makeVarchar("UTF8")},
                {"DEFAULT_COLLATION_NAME", TypedValue::makeVarchar("UTF8")},
                {"SQL_PATH", TypedValue()}
            };
            results.rows.push_back(row);
        }

        return Status::OK;
    }

    static std::string tableTypeName(CatalogManager::TableType type) {
        switch (type) {
            case CatalogManager::TableType::HEAP: return "BASE TABLE";
            case CatalogManager::TableType::TEMPORARY: return "LOCAL TEMPORARY";
            case CatalogManager::TableType::EXTERNAL: return "EXTERNAL";
            case CatalogManager::TableType::MATERIALIZED_VIEW: return "MATERIALIZED VIEW";
            case CatalogManager::TableType::INDEX: return "INDEX";
            case CatalogManager::TableType::TOAST: return "TOAST";
            default: return "BASE TABLE";
        }
    }

    Status queryTables(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status == Status::OK) {
                for (const auto& table : tables) {
                    VirtualRow row;
                    row.columns = {
                        {"TABLE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                        {"TABLE_NAME", TypedValue::makeVarchar(table.table_name)},
                        {"TABLE_TYPE", TypedValue::makeVarchar(tableTypeName(table.table_type))}
                    };
                    results.rows.push_back(row);
                }
            }

            std::vector<CatalogManager::ViewInfo> views;
            status = catalog_manager_->listViewsForSchema(schema.schema_id, views, ctx);
            if (status == Status::OK) {
                for (const auto& view : views) {
                    std::string type = view.materialized ? "MATERIALIZED VIEW" : "VIEW";
                    VirtualRow row;
                    row.columns = {
                        {"TABLE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                        {"TABLE_NAME", TypedValue::makeVarchar(view.name)},
                        {"TABLE_TYPE", TypedValue::makeVarchar(type)}
                    };
                    results.rows.push_back(row);
                }
            }
        }

        return Status::OK;
    }

    Status queryColumns(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
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

                for (const auto& column : columns) {
                    std::string domain_name;
                    if (column.domain_id != ID{}) {
                        DomainInfo domain_info;
                        if (catalog_manager_->getDomainById(column.domain_id, domain_info, ctx) == Status::OK) {
                            domain_name = domain_info.domain_name;
                        }
                    }

                    std::string default_value;
                    if (column.has_default) {
                        if (!column.default_value.empty()) {
                            default_value = column.default_value;
                        } else {
                            default_value = column.default_expr;
                        }
                    }

                    int64_t char_len = 0;
                    bool has_char_len = characterMaxLength(static_cast<DataType>(column.data_type),
                                                          column.type_precision, column.max_length,
                                                          char_len);

                    int64_t num_precision = 0;
                    bool has_num_precision = numericPrecision(static_cast<DataType>(column.data_type),
                                                              column.type_precision, num_precision);

                    int64_t num_scale = 0;
                    bool has_num_scale = numericScale(static_cast<DataType>(column.data_type),
                                                      column.type_scale, num_scale);

                    TypedValue identity_start;
                    TypedValue identity_increment;
                    TypedValue identity_max;
                    TypedValue identity_min;
                    std::string identity_cycle;
                    if (column.is_identity && column.identity_sequence_id != ID{}) {
                        CatalogManager::SequenceInfo seq;
                        if (catalog_manager_->getSequenceById(column.identity_sequence_id, seq, ctx) == Status::OK) {
                            identity_start = TypedValue::makeInt64(seq.start_value);
                            identity_increment = TypedValue::makeInt64(seq.increment_by);
                            identity_max = TypedValue::makeInt64(seq.max_value);
                            identity_min = TypedValue::makeInt64(seq.min_value);
                            identity_cycle = yesNo(seq.cycle);
                        }
                    }

                    VirtualRow row;
                    row.columns = {
                        {"TABLE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                        {"TABLE_NAME", TypedValue::makeVarchar(table.table_name)},
                        {"COLUMN_NAME", TypedValue::makeVarchar(column.column_name)},
                        {"ORDINAL_POSITION", TypedValue::makeInt64(column.ordinal)},
                        {"COLUMN_DEFAULT", default_value.empty() ? TypedValue() : TypedValue::makeVarchar(default_value)},
                        {"IS_NULLABLE", TypedValue::makeVarchar(yesNo(column.nullable))},
                        {"DATA_TYPE", TypedValue::makeVarchar(dataTypeName(static_cast<DataType>(column.data_type)))},
                        {"CHARACTER_MAXIMUM_LENGTH", has_char_len ? TypedValue::makeInt64(char_len) : TypedValue()},
                        {"NUMERIC_PRECISION", has_num_precision ? TypedValue::makeInt64(num_precision) : TypedValue()},
                        {"NUMERIC_SCALE", has_num_scale ? TypedValue::makeInt64(num_scale) : TypedValue()},
                        {"IS_IDENTITY", TypedValue::makeVarchar(yesNo(column.is_identity))},
                        {"IDENTITY_GENERATION", column.is_identity ? TypedValue::makeVarchar(column.identity_always ? "ALWAYS" : "BY DEFAULT") : TypedValue()},
                        {"IDENTITY_START", identity_start},
                        {"IDENTITY_INCREMENT", identity_increment},
                        {"IDENTITY_MAXIMUM", identity_max},
                        {"IDENTITY_MINIMUM", identity_min},
                        {"IDENTITY_CYCLE", identity_cycle.empty() ? TypedValue() : TypedValue::makeVarchar(identity_cycle)},
                        {"IS_GENERATED", TypedValue::makeVarchar(column.generated_type == CatalogManager::GeneratedColumnType::NOT_GENERATED ? "NEVER" : "ALWAYS")},
                        {"GENERATION_EXPRESSION", column.generation_expression.empty() ? TypedValue() : TypedValue::makeVarchar(column.generation_expression)},
                        {"DOMAIN_NAME", domain_name.empty() ? TypedValue() : TypedValue::makeVarchar(domain_name)}
                    };
                    results.rows.push_back(row);
                }
            }
        }

        return Status::OK;
    }

    Status queryViews(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::ViewInfo> views;
            status = catalog_manager_->listViewsForSchema(schema.schema_id, views, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& view : views) {
                VirtualRow row;
                row.columns = {
                    {"TABLE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                    {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                    {"TABLE_NAME", TypedValue::makeVarchar(view.name)},
                    {"VIEW_DEFINITION", view.definition.empty() ? TypedValue() : TypedValue::makeText(view.definition)}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    Status queryRoutines(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::FunctionInfo> functions;
        catalog_manager_->listFunctions(functions, ctx);
        for (const auto& func : functions) {
            std::string schema_name;
            if (!getSchemaPath(func.schema_id, schema_name)) {
                schema_name = "";
            }
            VirtualRow row;
            row.columns = {
                {"ROUTINE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                {"ROUTINE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                {"ROUTINE_NAME", TypedValue::makeVarchar(func.name)},
                {"ROUTINE_TYPE", TypedValue::makeVarchar("FUNCTION")},
                {"DATA_TYPE", TypedValue::makeVarchar(dataTypeName(func.return_type))}
            };
            results.rows.push_back(row);
        }

        std::vector<CatalogManager::ProcedureInfo> procedures;
        catalog_manager_->listProcedures(procedures, ctx);
        for (const auto& proc : procedures) {
            std::string schema_name;
            if (!getSchemaPath(proc.schema_id, schema_name)) {
                schema_name = "";
            }
            VirtualRow row;
            row.columns = {
                {"ROUTINE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                {"ROUTINE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                {"ROUTINE_NAME", TypedValue::makeVarchar(proc.name)},
                {"ROUTINE_TYPE", TypedValue::makeVarchar("PROCEDURE")},
                {"DATA_TYPE", TypedValue()}
            };
            results.rows.push_back(row);
        }

        return Status::OK;
    }

    Status queryParameters(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::FunctionInfo> functions;
        catalog_manager_->listFunctions(functions, ctx);
        for (const auto& func : functions) {
            std::string schema_name;
            if (!getSchemaPath(func.schema_id, schema_name)) {
                schema_name = "";
            }
            int64_t ordinal = 1;
            for (const auto& param : func.parameters) {
                VirtualRow row;
                row.columns = {
                    {"SPECIFIC_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                    {"SPECIFIC_SCHEMA", TypedValue::makeVarchar(schema_name)},
                    {"SPECIFIC_NAME", TypedValue::makeVarchar(func.name)},
                    {"ORDINAL_POSITION", TypedValue::makeInt64(ordinal++)},
                    {"PARAMETER_MODE", TypedValue::makeVarchar(parameterModeName(param.mode))},
                    {"PARAMETER_NAME", TypedValue::makeVarchar(param.name)},
                    {"DATA_TYPE", TypedValue::makeVarchar(dataTypeName(param.type))},
                    {"CHARACTER_MAXIMUM_LENGTH", TypedValue()},
                    {"NUMERIC_PRECISION", TypedValue()},
                    {"NUMERIC_SCALE", TypedValue()}
                };
                results.rows.push_back(row);
            }
        }

        std::vector<CatalogManager::ProcedureInfo> procedures;
        catalog_manager_->listProcedures(procedures, ctx);
        for (const auto& proc : procedures) {
            std::string schema_name;
            if (!getSchemaPath(proc.schema_id, schema_name)) {
                schema_name = "";
            }
            int64_t ordinal = 1;
            for (const auto& param : proc.parameters) {
                VirtualRow row;
                row.columns = {
                    {"SPECIFIC_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                    {"SPECIFIC_SCHEMA", TypedValue::makeVarchar(schema_name)},
                    {"SPECIFIC_NAME", TypedValue::makeVarchar(proc.name)},
                    {"ORDINAL_POSITION", TypedValue::makeInt64(ordinal++)},
                    {"PARAMETER_MODE", TypedValue::makeVarchar(parameterModeName(param.mode))},
                    {"PARAMETER_NAME", TypedValue::makeVarchar(param.name)},
                    {"DATA_TYPE", TypedValue::makeVarchar(dataTypeName(param.type))},
                    {"CHARACTER_MAXIMUM_LENGTH", TypedValue()},
                    {"NUMERIC_PRECISION", TypedValue()},
                    {"NUMERIC_SCALE", TypedValue()}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    static const char* parameterModeName(CatalogManager::ParameterMode mode) {
        switch (mode) {
            case CatalogManager::ParameterMode::IN: return "IN";
            case CatalogManager::ParameterMode::OUT: return "OUT";
            case CatalogManager::ParameterMode::INOUT: return "INOUT";
            default: return "IN";
        }
    }

    static std::string triggerEventName(CatalogManager::TriggerEvent event) {
        switch (event) {
            case CatalogManager::TriggerEvent::INSERT: return "INSERT";
            case CatalogManager::TriggerEvent::UPDATE: return "UPDATE";
            case CatalogManager::TriggerEvent::DELETE: return "DELETE";
            default: return "UNKNOWN";
        }
    }

    static std::string triggerTimingName(CatalogManager::TriggerTiming timing) {
        switch (timing) {
            case CatalogManager::TriggerTiming::BEFORE: return "BEFORE";
            case CatalogManager::TriggerTiming::AFTER: return "AFTER";
            default: return "BEFORE";
        }
    }

    Status queryTriggers(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::TriggerInfo> triggers;
                status = catalog_manager_->listAllTriggersForTable(table.table_id, triggers, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& trigger : triggers) {
                    auto emit_row = [&](const std::string& event_name) {
                        VirtualRow row;
                        row.columns = {
                            {"TRIGGER_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                            {"TRIGGER_SCHEMA", TypedValue::makeVarchar(schema_name)},
                            {"TRIGGER_NAME", TypedValue::makeVarchar(trigger.trigger_name)},
                            {"EVENT_MANIPULATION", TypedValue::makeVarchar(event_name)},
                            {"EVENT_OBJECT_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                            {"EVENT_OBJECT_SCHEMA", TypedValue::makeVarchar(schema_name)},
                            {"EVENT_OBJECT_TABLE", TypedValue::makeVarchar(table.table_name)},
                            {"ACTION_TIMING", TypedValue::makeVarchar(triggerTimingName(trigger.timing))},
                            {"ACTION_STATEMENT", trigger.procedure_name.empty() ? TypedValue() : TypedValue::makeText(trigger.procedure_name)}
                        };
                        results.rows.push_back(row);
                    };

                    const uint8_t mask = trigger.event_mask;
                    bool emitted = false;
                    for (CatalogManager::TriggerEvent event : {
                             CatalogManager::TriggerEvent::INSERT,
                             CatalogManager::TriggerEvent::UPDATE,
                             CatalogManager::TriggerEvent::DELETE}) {
                        const uint8_t bit = 1u << static_cast<uint8_t>(event);
                        if ((mask & bit) != 0) {
                            emit_row(triggerEventName(event));
                            emitted = true;
                        }
                    }
                    if (!emitted) {
                        emit_row("UNKNOWN");
                    }
                }
            }
        }

        return Status::OK;
    }

    Status querySequences(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::SequenceInfo> sequences;
            status = catalog_manager_->listSequences(schema.schema_id, sequences, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& seq : sequences) {
                VirtualRow row;
                row.columns = {
                    {"SEQUENCE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                    {"SEQUENCE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                    {"SEQUENCE_NAME", TypedValue::makeVarchar(seq.name)},
                    {"DATA_TYPE", TypedValue::makeVarchar("BIGINT")},
                    {"NUMERIC_PRECISION", TypedValue::makeInt64(64)},
                    {"NUMERIC_SCALE", TypedValue::makeInt64(0)},
                    {"START_VALUE", TypedValue::makeInt64(seq.start_value)},
                    {"MINIMUM_VALUE", TypedValue::makeInt64(seq.min_value)},
                    {"MAXIMUM_VALUE", TypedValue::makeInt64(seq.max_value)},
                    {"INCREMENT", TypedValue::makeInt64(seq.increment_by)},
                    {"CYCLE_OPTION", TypedValue::makeVarchar(yesNo(seq.cycle))}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    Status queryTableConstraints(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::ConstraintInfo> constraints;
                status = catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& constraint : constraints) {
                    VirtualRow row;
                    row.columns = {
                        {"CONSTRAINT_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"CONSTRAINT_SCHEMA", TypedValue::makeVarchar(schema_name)},
                        {"CONSTRAINT_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                        {"TABLE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                        {"TABLE_NAME", TypedValue::makeVarchar(table.table_name)},
                        {"CONSTRAINT_TYPE", TypedValue::makeVarchar(constraintTypeName(constraint.constraint_type))}
                    };
                    results.rows.push_back(row);
                }
            }
        }

        return Status::OK;
    }

    Status queryKeyColumnUsage(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::ConstraintInfo> constraints;
                status = catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& constraint : constraints) {
                    int64_t ordinal = 1;
                    for (const auto& col_name : constraint.column_names) {
                        VirtualRow row;
                        row.columns = {
                            {"CONSTRAINT_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                            {"CONSTRAINT_SCHEMA", TypedValue::makeVarchar(schema_name)},
                            {"CONSTRAINT_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                            {"TABLE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                            {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                            {"TABLE_NAME", TypedValue::makeVarchar(table.table_name)},
                            {"COLUMN_NAME", TypedValue::makeVarchar(col_name)},
                            {"ORDINAL_POSITION", TypedValue::makeInt64(ordinal++)},
                            {"POSITION_IN_UNIQUE_CONSTRAINT", TypedValue()}
                        };
                        results.rows.push_back(row);
                    }
                }
            }
        }

        return Status::OK;
    }

    Status queryReferentialConstraints(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::ConstraintInfo> constraints;
                status = catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& constraint : constraints) {
                    if (constraint.constraint_type != CatalogManager::ConstraintType::FOREIGN_KEY) {
                        continue;
                    }

                    std::string unique_schema;
                    std::string unique_constraint;
                    if (constraint.referenced_table_id != ID{}) {
                        CatalogManager::TableInfo ref_table;
                        if (catalog_manager_->getTable(constraint.referenced_table_id, ref_table, ctx) == Status::OK) {
                            getSchemaPath(ref_table.schema_id, unique_schema);
                            std::vector<CatalogManager::ConstraintInfo> ref_constraints;
                            if (catalog_manager_->getConstraintsForTable(ref_table.table_id, ref_constraints, ctx) == Status::OK) {
                                for (const auto& ref_constraint : ref_constraints) {
                                    if ((ref_constraint.constraint_type == CatalogManager::ConstraintType::PRIMARY_KEY ||
                                         ref_constraint.constraint_type == CatalogManager::ConstraintType::UNIQUE) &&
                                        ref_constraint.column_names == constraint.referenced_columns) {
                                        unique_constraint = ref_constraint.constraint_name;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    VirtualRow row;
                    row.columns = {
                        {"CONSTRAINT_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"CONSTRAINT_SCHEMA", TypedValue::makeVarchar(schema_name)},
                        {"CONSTRAINT_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                        {"UNIQUE_CONSTRAINT_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"UNIQUE_CONSTRAINT_SCHEMA", unique_schema.empty() ? TypedValue() : TypedValue::makeVarchar(unique_schema)},
                        {"UNIQUE_CONSTRAINT_NAME", unique_constraint.empty() ? TypedValue() : TypedValue::makeVarchar(unique_constraint)},
                        {"MATCH_OPTION", TypedValue::makeVarchar(fkMatchName(constraint.match_type))},
                        {"UPDATE_RULE", TypedValue::makeVarchar(fkActionName(constraint.on_update))},
                        {"DELETE_RULE", TypedValue::makeVarchar(fkActionName(constraint.on_delete))}
                    };
                    results.rows.push_back(row);
                }
            }
        }

        return Status::OK;
    }

    Status queryCheckConstraints(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::ConstraintInfo> constraints;
                status = catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& constraint : constraints) {
                    if (constraint.constraint_type != CatalogManager::ConstraintType::CHECK) {
                        continue;
                    }

                    std::string check_clause = constraint.check_expression;
                    if (check_clause.empty() && constraint.check_expr_oid != core::ID{}) {
                        catalog_manager_->loadStringFromToast(constraint.check_expr_oid, 0, check_clause, ctx);
                    }

                    VirtualRow row;
                    row.columns = {
                        {"CONSTRAINT_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                        {"CONSTRAINT_SCHEMA", TypedValue::makeVarchar(schema_name)},
                        {"CONSTRAINT_NAME", TypedValue::makeVarchar(constraint.constraint_name)},
                        {"CHECK_CLAUSE", check_clause.empty() ? TypedValue() : TypedValue::makeText(check_clause)}
                    };
                    results.rows.push_back(row);
                }
            }
        }

        return Status::OK;
    }

    Status queryStatistics(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
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

                std::vector<CatalogManager::ColumnInfo> columns;
                catalog_manager_->getColumns(table.table_id, columns, ctx);
                std::unordered_map<ID, std::string> column_id_name;
                for (const auto& col : columns) {
                    column_id_name[col.column_id] = col.column_name;
                }

                for (const auto& index : indexes) {
                    int64_t seq = 1;
                    for (const auto& col_id : index.column_ids) {
                        auto it = column_id_name.find(col_id);
                        if (it == column_id_name.end()) {
                            continue;
                        }
                        VirtualRow row;
                        row.columns = {
                            {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                            {"TABLE_NAME", TypedValue::makeVarchar(table.table_name)},
                            {"NON_UNIQUE", TypedValue::makeInt64(index.is_unique ? 0 : 1)},
                            {"INDEX_SCHEMA", TypedValue::makeVarchar(schema_name)},
                            {"INDEX_NAME", TypedValue::makeVarchar(index.index_name)},
                            {"SEQ_IN_INDEX", TypedValue::makeInt64(seq++)},
                            {"COLUMN_NAME", TypedValue::makeVarchar(it->second)},
                            {"COLLATION", TypedValue::makeVarchar("A")},
                            {"CARDINALITY", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
                            {"INDEX_TYPE", TypedValue::makeVarchar(indexTypeName(index.index_type))}
                        };
                        results.rows.push_back(row);
                    }
                }
            }
        }

        return Status::OK;
    }

    Status queryDomains(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<DomainInfo> domains;
        Status status = catalog_manager_->listDomains(ID{}, domains, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& domain : domains) {
            std::string schema_name;
            getSchemaPath(domain.schema_id, schema_name);

            int64_t char_len = 0;
            bool has_char_len = characterMaxLength(domain.base_type, domain.precision, 0, char_len);
            int64_t num_precision = 0;
            bool has_num_precision = numericPrecision(domain.base_type, domain.precision, num_precision);
            int64_t num_scale = 0;
            bool has_num_scale = numericScale(domain.base_type, domain.scale, num_scale);

            VirtualRow row;
            row.columns = {
                {"DOMAIN_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                {"DOMAIN_SCHEMA", schema_name.empty() ? TypedValue() : TypedValue::makeVarchar(schema_name)},
                {"DOMAIN_NAME", TypedValue::makeVarchar(domain.domain_name)},
                {"DATA_TYPE", TypedValue::makeVarchar(dataTypeName(domain.base_type))},
                {"CHARACTER_MAXIMUM_LENGTH", has_char_len ? TypedValue::makeInt64(char_len) : TypedValue()},
                {"NUMERIC_PRECISION", has_num_precision ? TypedValue::makeInt64(num_precision) : TypedValue()},
                {"NUMERIC_SCALE", has_num_scale ? TypedValue::makeInt64(num_scale) : TypedValue()},
                {"DOMAIN_DEFAULT", domain.default_value.empty() ? TypedValue() : TypedValue::makeVarchar(domain.default_value)},
                {"IS_NULLABLE", TypedValue::makeVarchar(yesNo(domain.nullable))}
            };
            results.rows.push_back(row);
        }

        return Status::OK;
    }

    Status queryUserDefinedTypes(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<DomainInfo> domains;
        Status status = catalog_manager_->listDomains(ID{}, domains, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& domain : domains) {
            std::string schema_name;
            getSchemaPath(domain.schema_id, schema_name);

            VirtualRow row;
            row.columns = {
                {"USER_DEFINED_TYPE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                {"USER_DEFINED_TYPE_SCHEMA", schema_name.empty() ? TypedValue() : TypedValue::makeVarchar(schema_name)},
                {"USER_DEFINED_TYPE_NAME", TypedValue::makeVarchar(domain.domain_name)},
                {"DATA_TYPE", TypedValue::makeVarchar(domainTypeName(domain.domain_type))},
                {"IS_ORDERABLE", TypedValue::makeVarchar("YES")}
            };
            results.rows.push_back(row);
        }

        return Status::OK;
    }

    Status queryTablePrivileges(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::PermissionInfo> permissions;
        Status status = catalog_manager_->listPermissions(permissions, ctx);
        if (status != Status::OK) {
            return Status::OK;
        }

        struct PrivBit { uint32_t mask; const char* name; };
        const PrivBit table_bits[] = {
            {static_cast<uint32_t>(CatalogManager::Privilege::SELECT), "SELECT"},
            {static_cast<uint32_t>(CatalogManager::Privilege::INSERT), "INSERT"},
            {static_cast<uint32_t>(CatalogManager::Privilege::UPDATE), "UPDATE"},
            {static_cast<uint32_t>(CatalogManager::Privilege::DELETE), "DELETE"},
            {static_cast<uint32_t>(CatalogManager::Privilege::TRUNCATE), "TRUNCATE"},
            {static_cast<uint32_t>(CatalogManager::Privilege::REFERENCES), "REFERENCES"},
            {static_cast<uint32_t>(CatalogManager::Privilege::TRIGGER), "TRIGGER"}
        };

        for (const auto& perm : permissions) {
            if (perm.object_type != CatalogManager::PermissionObjectType::TABLE &&
                perm.object_type != CatalogManager::PermissionObjectType::VIEW) {
                continue;
            }

            std::string schema_name;
            std::string object_name;
            if (perm.object_type == CatalogManager::PermissionObjectType::TABLE) {
                CatalogManager::TableInfo table_info;
                if (catalog_manager_->getTable(perm.object_id, table_info, ctx) != Status::OK) {
                    continue;
                }
                object_name = table_info.table_name;
                if (!getSchemaPath(table_info.schema_id, schema_name)) {
                    schema_name.clear();
                }
            } else {
                CatalogManager::ViewInfo view_info;
                if (catalog_manager_->getViewById(perm.object_id, view_info, ctx) != Status::OK) {
                    continue;
                }
                object_name = view_info.name;
                if (!getSchemaPath(view_info.schema_id, schema_name)) {
                    schema_name.clear();
                }
            }

            std::string grantee = resolveGranteeName(perm.grantee_type, perm.grantee_id);
            std::string grantor = resolveGrantorName(perm.grantor_id);

            for (const auto& bit : table_bits) {
                if ((perm.privileges & bit.mask) == 0) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"GRANTOR", TypedValue::makeVarchar(grantor)},
                    {"GRANTEE", TypedValue::makeVarchar(grantee)},
                    {"TABLE_CATALOG", TypedValue::makeVarchar(defaultCatalogName())},
                    {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                    {"TABLE_NAME", TypedValue::makeVarchar(object_name)},
                    {"PRIVILEGE_TYPE", TypedValue::makeVarchar(bit.name)},
                    {"IS_GRANTABLE", TypedValue::makeVarchar(yesNo(perm.grant_option))}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    Status querySchemaPrivileges(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::PermissionInfo> permissions;
        Status status = catalog_manager_->listPermissions(permissions, ctx);
        if (status != Status::OK) {
            return Status::OK;
        }

        struct PrivBit { uint32_t mask; const char* name; };
        const PrivBit schema_bits[] = {
            {static_cast<uint32_t>(CatalogManager::Privilege::CREATE), "CREATE"},
            {static_cast<uint32_t>(CatalogManager::Privilege::USAGE), "USAGE"}
        };

        for (const auto& perm : permissions) {
            if (perm.object_type != CatalogManager::PermissionObjectType::SCHEMA) {
                continue;
            }

            CatalogManager::SchemaInfo schema_info;
            if (catalog_manager_->getSchema(perm.object_id, schema_info, ctx) != Status::OK) {
                continue;
            }

            std::string schema_name = schema_info.full_path.empty() ? schema_info.schema_name : schema_info.full_path;
            std::string grantee = resolveGranteeName(perm.grantee_type, perm.grantee_id);
            std::string grantor = resolveGrantorName(perm.grantor_id);

            for (const auto& bit : schema_bits) {
                if ((perm.privileges & bit.mask) == 0) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"GRANTOR", TypedValue::makeVarchar(grantor)},
                    {"GRANTEE", TypedValue::makeVarchar(grantee)},
                    {"SCHEMA_NAME", TypedValue::makeVarchar(schema_name)},
                    {"PRIVILEGE_TYPE", TypedValue::makeVarchar(bit.name)},
                    {"IS_GRANTABLE", TypedValue::makeVarchar(yesNo(perm.grant_option))}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    Status queryDatabasePrivileges(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::PermissionInfo> permissions;
        Status status = catalog_manager_->listPermissions(permissions, ctx);
        if (status != Status::OK) {
            return Status::OK;
        }

        struct PrivBit { uint32_t mask; const char* name; };
        const PrivBit db_bits[] = {
            {static_cast<uint32_t>(CatalogManager::Privilege::CONNECT), "CONNECT"},
            {static_cast<uint32_t>(CatalogManager::Privilege::CREATE), "CREATE"},
            {static_cast<uint32_t>(CatalogManager::Privilege::TEMPORARY), "TEMPORARY"}
        };

        for (const auto& perm : permissions) {
            if (perm.object_type != CatalogManager::PermissionObjectType::DATABASE) {
                continue;
            }

            std::string grantee = resolveGranteeName(perm.grantee_type, perm.grantee_id);
            std::string grantor = resolveGrantorName(perm.grantor_id);

            for (const auto& bit : db_bits) {
                if ((perm.privileges & bit.mask) == 0) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"GRANTOR", TypedValue::makeVarchar(grantor)},
                    {"GRANTEE", TypedValue::makeVarchar(grantee)},
                    {"DATABASE_NAME", TypedValue::makeVarchar("default")},
                    {"PRIVILEGE_TYPE", TypedValue::makeVarchar(bit.name)},
                    {"IS_GRANTABLE", TypedValue::makeVarchar(yesNo(perm.grant_option))}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    Status queryUsagePrivileges(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::PermissionInfo> permissions;
        Status status = catalog_manager_->listPermissions(permissions, ctx);
        if (status != Status::OK) {
            return Status::OK;
        }

        struct PrivBit { uint32_t mask; const char* name; };
        const PrivBit seq_bits[] = {
            {static_cast<uint32_t>(CatalogManager::Privilege::SEQUENCE_USAGE), "USAGE"},
            {static_cast<uint32_t>(CatalogManager::Privilege::SEQUENCE_UPDATE), "UPDATE"}
        };

        for (const auto& perm : permissions) {
            if (perm.object_type != CatalogManager::PermissionObjectType::SEQUENCE) {
                continue;
            }

            CatalogManager::SequenceInfo seq;
            if (catalog_manager_->getSequenceById(perm.object_id, seq, ctx) != Status::OK) {
                continue;
            }

            std::string schema_name;
            getSchemaPath(seq.schema_id, schema_name);

            std::string grantee = resolveGranteeName(perm.grantee_type, perm.grantee_id);
            std::string grantor = resolveGrantorName(perm.grantor_id);

            for (const auto& bit : seq_bits) {
                if ((perm.privileges & bit.mask) == 0) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"GRANTOR", TypedValue::makeVarchar(grantor)},
                    {"GRANTEE", TypedValue::makeVarchar(grantee)},
                    {"OBJECT_SCHEMA", TypedValue::makeVarchar(schema_name)},
                    {"OBJECT_NAME", TypedValue::makeVarchar(seq.name)},
                    {"PRIVILEGE_TYPE", TypedValue::makeVarchar(bit.name)},
                    {"IS_GRANTABLE", TypedValue::makeVarchar(yesNo(perm.grant_option))}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    Status queryRoutinePrivileges(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::PermissionInfo> permissions;
        Status status = catalog_manager_->listPermissions(permissions, ctx);
        if (status != Status::OK) {
            return Status::OK;
        }

        struct PrivBit { uint32_t mask; const char* name; };
        const PrivBit exec_bits[] = {
            {static_cast<uint32_t>(CatalogManager::Privilege::EXECUTE), "EXECUTE"}
        };

        std::unordered_map<ID, CatalogManager::FunctionInfo> function_map;
        std::vector<CatalogManager::FunctionInfo> functions;
        catalog_manager_->listFunctions(functions, ctx);
        for (const auto& func : functions) {
            function_map.emplace(func.function_id, func);
        }

        std::unordered_map<ID, CatalogManager::ProcedureInfo> procedure_map;
        std::vector<CatalogManager::ProcedureInfo> procedures;
        catalog_manager_->listProcedures(procedures, ctx);
        for (const auto& proc : procedures) {
            procedure_map.emplace(proc.procedure_id, proc);
        }

        for (const auto& perm : permissions) {
            if (perm.object_type != CatalogManager::PermissionObjectType::FUNCTION &&
                perm.object_type != CatalogManager::PermissionObjectType::PROCEDURE) {
                continue;
            }

            std::string schema_name;
            std::string routine_name;
            if (perm.object_type == CatalogManager::PermissionObjectType::FUNCTION) {
                auto it = function_map.find(perm.object_id);
                if (it == function_map.end()) {
                    continue;
                }
                getSchemaPath(it->second.schema_id, schema_name);
                routine_name = it->second.name;
            } else {
                auto it = procedure_map.find(perm.object_id);
                if (it == procedure_map.end()) {
                    continue;
                }
                getSchemaPath(it->second.schema_id, schema_name);
                routine_name = it->second.name;
            }

            std::string grantee = resolveGranteeName(perm.grantee_type, perm.grantee_id);
            std::string grantor = resolveGrantorName(perm.grantor_id);

            for (const auto& bit : exec_bits) {
                if ((perm.privileges & bit.mask) == 0) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"GRANTOR", TypedValue::makeVarchar(grantor)},
                    {"GRANTEE", TypedValue::makeVarchar(grantee)},
                    {"ROUTINE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                    {"ROUTINE_NAME", TypedValue::makeVarchar(routine_name)},
                    {"PRIVILEGE_TYPE", TypedValue::makeVarchar(bit.name)},
                    {"IS_GRANTABLE", TypedValue::makeVarchar(yesNo(perm.grant_option))}
                };
                results.rows.push_back(row);
            }
        }

        return Status::OK;
    }

    Status queryColumnPrivileges(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        struct PrivBit { uint32_t mask; const char* name; };
        const PrivBit col_bits[] = {
            {static_cast<uint32_t>(CatalogManager::Privilege::SELECT), "SELECT"},
            {static_cast<uint32_t>(CatalogManager::Privilege::INSERT), "INSERT"},
            {static_cast<uint32_t>(CatalogManager::Privilege::UPDATE), "UPDATE"},
            {static_cast<uint32_t>(CatalogManager::Privilege::REFERENCES), "REFERENCES"}
        };

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schema.full_path.empty() ? schema.schema_name : schema.full_path;
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::ColumnPermissionInfo> perms;
                status = catalog_manager_->getColumnPermissions(table.table_id, perms, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& perm : perms) {
                    std::string grantee = resolveGranteeName(perm.grantee_type, perm.grantee_id);
                    std::string grantor = resolveGrantorName(perm.grantor_id);

                    for (const auto& bit : col_bits) {
                        if ((perm.privileges & bit.mask) == 0) {
                            continue;
                        }
                        VirtualRow row;
                        row.columns = {
                            {"GRANTOR", TypedValue::makeVarchar(grantor)},
                            {"GRANTEE", TypedValue::makeVarchar(grantee)},
                            {"TABLE_SCHEMA", TypedValue::makeVarchar(schema_name)},
                            {"TABLE_NAME", TypedValue::makeVarchar(table.table_name)},
                            {"COLUMN_NAME", TypedValue::makeVarchar(perm.column_name)},
                            {"PRIVILEGE_TYPE", TypedValue::makeVarchar(bit.name)},
                            {"IS_GRANTABLE", TypedValue::makeVarchar(yesNo(perm.grant_option))}
                        };
                        results.rows.push_back(row);
                    }
                }
            }
        }

        return Status::OK;
    }

    Status queryProcesslist(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<ProcessControlBlock> backends;
        core::ErrorContext proc_ctx;
        if (ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) != Status::OK) {
            return Status::OK;
        }

        std::unordered_map<ID, CatalogManager::SessionInfo, IDHash> sessions_by_id;
        std::vector<CatalogManager::SessionInfo> sessions;
        if (catalog_manager_->listSessions(sessions, ctx) == Status::OK) {
            for (const auto& session : sessions) {
                sessions_by_id[session.session_id] = session;
            }
        }

        auto now_micros = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        for (const auto& backend : backends) {
            const CatalogManager::SessionInfo* session = nullptr;
            auto it = sessions_by_id.find(backend.session_id);
            if (it != sessions_by_id.end()) {
                session = &it->second;
            }

            uint64_t base_time = backend.state_change_time != 0
                ? backend.state_change_time
                : backend.start_time;
            if (backend.query_start_time != 0) {
                base_time = backend.query_start_time;
            }

            uint64_t elapsed_seconds = 0;
            if (base_time > 0 && now_micros >= base_time) {
                elapsed_seconds = (now_micros - base_time) / 1000000ULL;
            }

            std::string command = backend.query_start_time != 0 ? "Query" : "Sleep";
            TypedValue info;
            if (backend.query_start_time != 0 && backend.query_text[0] != '\0') {
                info = TypedValue::makeText(backend.query_text);
            }

            std::string user = session ? session->username : "unknown";

            VirtualRow row;
            row.columns = {
                {"ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id))},
                {"USER", TypedValue::makeVarchar(user)},
                {"HOST", TypedValue::makeVarchar("local")},
                {"DB", TypedValue::makeVarchar("scratchbird")},
                {"COMMAND", TypedValue::makeVarchar(command)},
                {"TIME", TypedValue::makeInt64(static_cast<int64_t>(elapsed_seconds))},
                {"STATE", backend.query_start_time != 0 ? TypedValue::makeVarchar("executing") : TypedValue()},
                {"INFO", info}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }
};

} // namespace scratchbird::catalog
