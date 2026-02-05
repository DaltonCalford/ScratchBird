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
 * ScratchBird SBLR v2.0 - Semantic Analyzer
 *
 * This module performs semantic analysis on the unresolved AST from Parser v2,
 * producing a resolved AST with:
 * - Name resolution (tables, columns, functions resolved to UUIDs)
 * - Type checking for all expressions
 * - Validation of SQL semantics
 *
 * Design:
 * - Visitor pattern over unresolved AST
 * - Uses CatalogManager for name lookups
 * - Maintains scope stack for nested contexts (subqueries, CTEs)
 * - Produces ResolvedStatement output
 *
 * See: docs/planning/PARSER_V2_IMPLEMENTATION_PLAN.md Section 8.5
 */

#include "scratchbird/parser/ast_v2.h"
#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/resolved_ast_v2.h"
#include "scratchbird/core/catalog_manager.h"
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <unordered_map>
#include <optional>

namespace scratchbird::parser::v2 {

// Import types from core namespace
using core::CatalogManager;
using core::Status;

/**
 * Semantic analysis error
 */
struct SemanticError {
    SourceSpan span;
    std::string message;
    std::string hint;

    enum class Severity {
        ERROR,
        WARNING,
    };
    Severity severity = Severity::ERROR;
};

/**
 * Result of semantic analysis
 */
class SemanticResult {
public:
    SemanticResult() = default;

    bool success() const { return errors_.empty() && statement_ != nullptr; }
    ResolvedStatement* statement() const { return statement_; }
    const std::vector<SemanticError>& errors() const { return errors_; }
    const std::vector<SemanticError>& warnings() const { return warnings_; }
    StringPool* stringPool() const { return string_pool_; }
    const std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>>& dependencies() const { return dependencies_; }

    void setStatement(ResolvedStatement* stmt) { statement_ = stmt; }
    void addError(const SemanticError& error);
    void addWarning(const SemanticError& warning);
    void setStringPool(StringPool* pool) { string_pool_ = pool; }
    void addDependency(const core::ID& id, core::CatalogManager::ObjectType type) {
        dependencies_.emplace_back(id, type);
    }

private:
    ResolvedStatement* statement_ = nullptr;
    std::vector<SemanticError> errors_;
    std::vector<SemanticError> warnings_;
    StringPool* string_pool_ = nullptr;
    std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>> dependencies_;
};

/**
 * Name resolution scope
 *
 * Maintains the current naming context for resolving identifiers.
 * Scopes are stacked for nested queries (subqueries, CTEs).
 */
class ResolutionScope {
public:
    ResolutionScope() = default;

    /**
     * Table entry in scope (from FROM clause or CTE)
     */
    struct TableEntry {
        StringPool::StringId alias;             // Alias or table name
        ID table_uuid;                          // Resolved UUID
        ResolvedTableRef* resolved_ref = nullptr;
        bool is_cte = false;

        // Column information for this table
        std::vector<ResolvedTableRef::ColumnInfo> columns;
    };

    /**
     * Add a table to the current scope
     */
    void addTable(const TableEntry& entry);

    /**
     * Look up a table by alias/name
     */
    const TableEntry* findTable(StringPool::StringId name) const;

    /**
     * Look up a column by name (searches all tables in scope)
     * Returns nullptr if not found or ambiguous
     */
    struct ColumnLookupResult {
        const TableEntry* table = nullptr;
        const ResolvedTableRef::ColumnInfo* column = nullptr;
        bool ambiguous = false;
    };
    ColumnLookupResult findColumn(StringPool::StringId name) const;

    /**
     * Look up a column with explicit table qualifier
     */
    const ResolvedTableRef::ColumnInfo* findColumn(
        StringPool::StringId table_name,
        StringPool::StringId column_name) const;

    /**
     * Get all tables in scope
     */
    const std::vector<TableEntry>& tables() const { return tables_; }

    /**
     * Clear the scope
     */
    void clear();

private:
    std::vector<TableEntry> tables_;
    std::unordered_map<StringPool::StringId, size_t> table_map_;  // alias -> index
};

/**
 * Semantic Analyzer V2
 *
 * Analyzes unresolved AST and produces resolved AST with:
 * - Name resolution to catalog UUIDs
 * - Type information on all expressions
 * - Semantic validation
 */
class SemanticAnalyzerV2 {
public:
    /**
     * Create analyzer with catalog and string pool access
     */
    SemanticAnalyzerV2(CatalogManager& catalog, StringPool& string_pool);
    ~SemanticAnalyzerV2();

    // Non-copyable
    SemanticAnalyzerV2(const SemanticAnalyzerV2&) = delete;
    SemanticAnalyzerV2& operator=(const SemanticAnalyzerV2&) = delete;

    /**
     * Analyze a statement and produce resolved AST
     * @param stmt Unresolved statement from parser
     * @return SemanticResult containing resolved statement or errors
     */
    SemanticResult analyze(Statement* stmt);

    /**
     * Set the current schema for unqualified name resolution
     */
    void setCurrentSchema(const ID& schema_id) { current_schema_ = schema_id; }

    /**
     * Set the schema search path
     */
    void setSearchPath(const std::vector<ID>& path) { search_path_ = path; }

    /**
     * Access to resolved AST arena
     */
    ResolvedASTArena& arena() { return arena_; }

    /**
     * Seed PSQL variable scope (procedure/function params) for nested PSQL analysis.
     */
    void seedPsqlVariable(std::string_view name, const ResolvedType& type);

private:
    CatalogManager& catalog_;
    StringPool& string_pool_;
    ResolvedASTArena arena_;

    // Current analysis context
    SemanticResult* current_result_ = nullptr;
    ID current_schema_;
    std::vector<ID> search_path_;

    // Scope stack for nested contexts
    std::vector<ResolutionScope> scope_stack_;

    struct CTEEntry {
        StringPool::StringId name = StringPool::INVALID_ID;
        std::vector<ResolvedTableRef::ColumnInfo> columns;
        ResolvedSelectStmt* query = nullptr;
    };

    std::vector<std::unordered_map<StringPool::StringId, CTEEntry>> cte_scopes_;

    // Analysis state
    bool in_aggregate_ = false;
    bool has_aggregates_ = false;
    int subquery_depth_ = 0;
    bool in_psql_ = false;

    struct PsqlVarEntry {
        StringPool::StringId name = StringPool::INVALID_ID;
        ResolvedType type;
    };

    std::vector<std::unordered_map<std::string, PsqlVarEntry>> psql_var_scopes_;

    // ==========================================================================
    // Error Handling
    // ==========================================================================

    void error(SourceSpan span, const std::string& message, const std::string& hint = "");
    void warning(SourceSpan span, const std::string& message, const std::string& hint = "");

    // ==========================================================================
    // Scope Management
    // ==========================================================================

    void pushScope();
    void popScope();
    ResolutionScope& currentScope();
    void pushCTEScope();
    void popCTEScope();
    void registerCTE(const CTEEntry& entry);
    const CTEEntry* findCTE(StringPool::StringId name);

    ResolvedWithClause* analyzeWithClause(WithClause* with);
    void applyCTEColumnAliases(ResolvedSelectStmt* query,
                               const std::vector<StringPool::StringId>& column_names,
                               SourceSpan span);
    std::vector<ResolvedTableRef::ColumnInfo> buildCTEColumns(
        ResolvedSelectStmt* query,
        const std::vector<StringPool::StringId>& column_names);

    // ==========================================================================
    // Name Resolution
    // ==========================================================================

    /**
     * Resolve a schema path to a table UUID
     */
    std::optional<ResolvedTableRef> resolveTable(const SchemaPath& path, SourceSpan span,
                                                 bool allow_search_path = true);

    /**
     * Resolve a column reference
     */
    std::optional<ResolvedColumnRef> resolveColumn(
        StringPool::StringId table_alias,
        StringPool::StringId column_name,
        SourceSpan span);

    // ==========================================================================
    // PSQL Variable Scope
    // ==========================================================================

    void pushPsqlScope();
    void popPsqlScope();
    void declarePsqlVariable(StringPool::StringId name, const ResolvedType& type);
    const PsqlVarEntry* findPsqlVariable(StringPool::StringId name) const;

    /**
     * Resolve a function name
     */
    std::optional<ResolvedFunctionRef> resolveFunction(
        const SchemaPath& path,
        const std::vector<ResolvedType>& arg_types,
        SourceSpan span);

    /**
     * Load column information for a resolved table
     */
    bool loadTableColumns(ResolvedTableRef& ref);

    // ==========================================================================
    // Type Checking
    // ==========================================================================

    /**
     * Get common type for binary operation
     */
    std::optional<ResolvedType> getCommonType(
        const ResolvedType& left,
        const ResolvedType& right,
        BinaryOp op);

    /**
     * Check if implicit cast is allowed
     */
    bool canImplicitCast(const ResolvedType& from, const ResolvedType& to);

    /**
     * Insert implicit cast if needed
     */
    ResolvedExpression* insertImplicitCast(
        ResolvedExpression* expr,
        const ResolvedType& target_type);

    // ==========================================================================
    // Statement Analysis
    // ==========================================================================

    ResolvedStatement* analyzeStatement(Statement* stmt);

    // DDL
    ResolvedStatement* analyzeCreateTable(CreateTableStmt* stmt);
    ResolvedStatement* analyzeCreateIndex(CreateIndexStmt* stmt);
    ResolvedStatement* analyzeCreateView(CreateViewStmt* stmt);
    ResolvedStatement* analyzeCreateSequence(CreateSequenceStmt* stmt);
    ResolvedStatement* analyzeAlterSequence(AlterSequenceStmt* stmt);
    ResolvedStatement* analyzeCreateSchema(CreateSchemaStmt* stmt);
    ResolvedStatement* analyzeDropSchema(DropSchemaStmt* stmt);
    ResolvedStatement* analyzeAlterSchema(AlterSchemaStmt* stmt);
    ResolvedStatement* analyzeCreateDatabase(CreateDatabaseStmt* stmt);
    ResolvedStatement* analyzeCreateTablespace(CreateTablespaceStmt* stmt);
    ResolvedStatement* analyzeAlterTablespace(AlterTablespaceStmt* stmt);
    ResolvedStatement* analyzeDropTablespace(DropTablespaceStmt* stmt);
    ResolvedStatement* analyzeAttachTablespace(AttachTablespaceStmt* stmt);
    ResolvedStatement* analyzeDetachTablespace(DetachTablespaceStmt* stmt);
    ResolvedStatement* analyzeCreateFunction(CreateFunctionStmt* stmt);
    ResolvedStatement* analyzeCreateProcedure(CreateProcedureStmt* stmt);
    ResolvedStatement* analyzeCreateTrigger(CreateTriggerStmt* stmt);
    ResolvedStatement* analyzeExecuteBlock(ExecuteBlockStmt* stmt);
    ResolvedStatement* analyzeExecuteProcedure(ExecuteProcedureStmt* stmt);
    ResolvedStatement* analyzeExecuteStatement(ExecuteStatementStmt* stmt);
    ResolvedStatement* analyzeCreatePackage(CreatePackageStmt* stmt);
    ResolvedStatement* analyzeCreateUser(CreateUserStmt* stmt);
    ResolvedStatement* analyzeCreateRole(CreateRoleStmt* stmt);
    ResolvedStatement* analyzeCreateGroup(CreateGroupStmt* stmt);
    ResolvedStatement* analyzeCreatePolicy(CreatePolicyStmt* stmt);
    ResolvedStatement* analyzeCreateJob(CreateJobStmt* stmt);
    ResolvedStatement* analyzeCreateException(CreateExceptionStmt* stmt);
    ResolvedStatement* analyzeCreateType(CreateTypeStmt* stmt);
    ResolvedStatement* analyzeCreateDomain(CreateDomainStmt* stmt);
    ResolvedStatement* analyzeCreateForeignServer(CreateForeignServerStmt* stmt);
    ResolvedStatement* analyzeCreateForeignTable(CreateForeignTableStmt* stmt);
    ResolvedStatement* analyzeCreateUserMapping(CreateUserMappingStmt* stmt);
    ResolvedStatement* analyzeCreateSynonym(CreateSynonymStmt* stmt);
    ResolvedStatement* analyzeCreateUdr(CreateUdrStmt* stmt);
    ResolvedStatement* analyzeDropDatabase(DropDatabaseStmt* stmt);
    ResolvedStatement* analyzeAlterDatabase(AlterDatabaseStmt* stmt);
    ResolvedStatement* analyzeAlterType(AlterTypeStmt* stmt);
    ResolvedStatement* analyzeAlterDomain(AlterDomainStmt* stmt);
    ResolvedStatement* analyzeAlterPolicy(AlterPolicyStmt* stmt);
    ResolvedStatement* analyzeDropDomain(DropDomainStmt* stmt);
    ResolvedStatement* analyzeDropPolicy(DropPolicyStmt* stmt);
    ResolvedStatement* analyzeAlterTable(AlterTableStmt* stmt);
    ResolvedStatement* analyzeAlterIndex(AlterIndexStmt* stmt);
    ResolvedStatement* analyzeRenameObject(RenameObjectStmt* stmt);
    ResolvedStatement* analyzeMoveObject(MoveObjectStmt* stmt);
    ResolvedStatement* analyzeDropTable(DropTableStmt* stmt);
    ResolvedStatement* analyzeDropIndex(DropIndexStmt* stmt);
    ResolvedStatement* analyzeDropView(DropViewStmt* stmt);
    ResolvedStatement* analyzeDropSequence(DropSequenceStmt* stmt);
    ResolvedStatement* analyzeDropFunction(DropFunctionStmt* stmt);
    ResolvedStatement* analyzeDropProcedure(DropProcedureStmt* stmt);
    ResolvedStatement* analyzeDropTrigger(DropTriggerStmt* stmt);
    ResolvedStatement* analyzeDropPackage(DropPackageStmt* stmt);
    ResolvedStatement* analyzeDropRole(DropRoleStmt* stmt);
    ResolvedStatement* analyzeDropGroup(DropGroupStmt* stmt);
    ResolvedStatement* analyzeDropJob(DropJobStmt* stmt);
    ResolvedStatement* analyzeDropException(DropExceptionStmt* stmt);
    ResolvedStatement* analyzeDropForeignServer(DropForeignServerStmt* stmt);
    ResolvedStatement* analyzeDropForeignTable(DropForeignTableStmt* stmt);
    ResolvedStatement* analyzeDropUserMapping(DropUserMappingStmt* stmt);
    ResolvedStatement* analyzeDropSynonym(DropSynonymStmt* stmt);
    ResolvedStatement* analyzeDropUdr(DropUdrStmt* stmt);
    ResolvedStatement* analyzeDropType(DropTypeStmt* stmt);
    ResolvedStatement* analyzeAlterJob(AlterJobStmt* stmt);
    ResolvedStatement* analyzeExecuteJob(ExecuteJobStmt* stmt);
    ResolvedStatement* analyzeCancelJobRun(CancelJobRunStmt* stmt);
    ResolvedStatement* analyzeTruncateTable(TruncateTableStmt* stmt);

    // DML
    ResolvedSelectStmt* analyzeSelect(SelectStmt* stmt);
    ResolvedStatement* analyzeInsert(InsertStmt* stmt);
    ResolvedStatement* analyzeUpdate(UpdateStmt* stmt);
    ResolvedStatement* analyzeDelete(DeleteStmt* stmt);
    ResolvedStatement* analyzeMerge(MergeStmt* stmt);
    ResolvedStatement* analyzeCopy(CopyStmt* stmt);
    ResolvedStatement* analyzeComment(CommentStmt* stmt);

    // Transaction/Session
    ResolvedStatement* analyzeStartTransaction(StartTransactionStmt* stmt);
    ResolvedStatement* analyzePrepareTransaction(PrepareTransactionStmt* stmt);
    ResolvedStatement* analyzeCommit(CommitStmt* stmt);
    ResolvedStatement* analyzeRollback(RollbackStmt* stmt);
    ResolvedStatement* analyzeSavepoint(SavepointStmt* stmt);
    ResolvedStatement* analyzeReleaseSavepoint(ReleaseSavepointStmt* stmt);
    ResolvedStatement* analyzeConnect(ConnectStmt* stmt);
    ResolvedStatement* analyzeDisconnect(DisconnectStmt* stmt);
    ResolvedStatement* analyzeSet(SetStmt* stmt);
    ResolvedStatement* analyzeReset(ResetStmt* stmt);
    ResolvedStatement* analyzeAlterSystem(AlterSystemStmt* stmt);
    ResolvedStatement* analyzeShow(ShowStmt* stmt);
    ResolvedStatement* analyzeExplain(ExplainStmt* stmt);
    ResolvedStatement* analyzeAnalyze(AnalyzeStmt* stmt);
    ResolvedStatement* analyzeSweepDatabase(SweepDatabaseStmt* stmt);

    // DCL
    ResolvedStatement* analyzeGrant(GrantStmt* stmt);
    ResolvedStatement* analyzeRevoke(RevokeStmt* stmt);

    // ==========================================================================
    // Expression Analysis
    // ==========================================================================

    ResolvedExpression* analyzeExpression(Expression* expr);

    ResolvedExpression* analyzeLiteral(LiteralExpr* expr);
    ResolvedExpression* analyzeColumnRef(ColumnRefExpr* expr);
    ResolvedExpression* analyzeParameter(ParameterExpr* expr);

    // PSQL helpers
    ResolvedPsqlStatement* analyzePsqlStatement(Statement* stmt);
    ResolvedPsqlBlock* analyzePsqlBlock(CompoundStmt* stmt,
                                        const std::vector<VariableDecl>& variables);
    ResolvedExpression* analyzeBinaryExpr(BinaryExpr* expr);
    ResolvedExpression* analyzeUnaryExpr(UnaryExpr* expr);
    ResolvedExpression* analyzeFunctionCall(FunctionCallExpr* expr);
    ResolvedExpression* analyzeCast(CastExpr* expr);
    ResolvedExpression* analyzeExtract(ExtractExpr* expr);
    ResolvedExpression* analyzeAlterElement(AlterElementExpr* expr);
    ResolvedExpression* analyzeCase(CaseExpr* expr);
    ResolvedExpression* analyzeSubquery(SubqueryExpr* expr);
    ResolvedExpression* analyzeExists(ExistsExpr* expr);
    ResolvedExpression* analyzeIn(InExpr* expr);
    ResolvedExpression* analyzeBetween(BetweenExpr* expr);
    ResolvedExpression* analyzeLike(LikeExpr* expr);
    ResolvedExpression* analyzeIsNull(IsNullExpr* expr);
    ResolvedExpression* analyzeArray(ArrayExpr* expr);

    // ==========================================================================
    // Clause Analysis
    // ==========================================================================

    void analyzeFromClause(SelectStmt* stmt, ResolvedSelectStmt* resolved);
    void analyzeWhereClause(Expression* where, ResolvedExpression*& resolved);
    void analyzeGroupByClause(SelectStmt* stmt, ResolvedSelectStmt* resolved);
    void analyzeHavingClause(Expression* having, ResolvedExpression*& resolved);
    void analyzeOrderByClause(const std::vector<OrderByItem*>& items,
                              std::vector<ResolvedOrderByItem*>& resolved,
                              const std::unordered_map<std::string, ResolvedExpression*>* alias_map = nullptr);
    void analyzeSelectList(SelectStmt* stmt, ResolvedSelectStmt* resolved);
    void analyzeReturningClause(const std::vector<SelectItem*>& returning,
                                std::vector<ResolvedSelectItem>& resolved);

    // ==========================================================================
    // Table Reference Analysis
    // ==========================================================================

    ResolvedTableRef* analyzeTableRef(TableRefNode* node);
    ResolvedJoin* analyzeJoin(JoinNode* node);

    // ==========================================================================
    // Type Resolution Helpers
    // ==========================================================================

    ResolvedType resolveTypeName(const TypeName& type_name);
    core::CastFormat resolveCastFormat(const CastExpr* expr);
    DataType mapToDataType(DataType ast_type, int32_t precision, int32_t scale);

    // ==========================================================================
    // Column Definition Analysis
    // ==========================================================================

    ResolvedColumnDef analyzeColumnDef(ColumnDef* def);
    ResolvedTableConstraint analyzeTableConstraint(TableConstraint* constraint,
                                                   const std::vector<ResolvedColumnDef>& columns);

    // ==========================================================================
    // Utility Methods
    // ==========================================================================

    /**
     * Get string from pool
     */
    std::string_view getString(StringPool::StringId id) const;

    /**
     * Create a StringPool entry
     */
    StringPool::StringId internString(std::string_view str);

    /**
     * Check if expression is aggregate
     */
    bool isAggregate(const ResolvedExpression* expr) const;

    /**
     * Validate GROUP BY semantics
     */
    bool validateGroupBy(ResolvedSelectStmt* stmt);
};

/**
 * Convenience function to analyze SQL with v2 parser
 */
inline SemanticResult analyzeSQL(
    std::string_view sql,
    CatalogManager& catalog,
    StringPool& string_pool)
{
    Parser parser(sql);
    auto parse_result = parser.parseStatement();

    if (!parse_result.success()) {
        SemanticResult result;
        for (const auto& err : parse_result.errors()) {
            SemanticError sem_err;
            sem_err.span = err.span;
            sem_err.message = err.message;
            sem_err.hint = err.hint;
            sem_err.severity = SemanticError::Severity::ERROR;
            result.addError(sem_err);
        }
        return result;
    }

    SemanticAnalyzerV2 analyzer(catalog, string_pool);
    return analyzer.analyze(parse_result.statement());
}

} // namespace scratchbird::parser::v2
