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
 * ScratchBird Query Compiler V2
 *
 * Phase 9: Integration layer for Parser V2 pipeline
 *
 * This class wraps the complete Parser V2 compilation pipeline:
 * - Lexer V2 (tokenization)
 * - Parser V2 (AST generation)
 * - Semantic Analyzer V2 (name resolution, type checking)
 * - Bytecode Generator V2 (SBLR bytecode)
 *
 * Features:
 * - Query result caching (via QueryResultCache)
 * - Prepared statement support
 * - Compilation statistics
 */

#include "scratchbird/parser/parser_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/query_result_cache.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>

namespace scratchbird {
namespace sblr {

/**
 * Compilation statistics
 */
struct CompilationStats {
    std::chrono::microseconds lexer_time{0};
    std::chrono::microseconds parser_time{0};
    std::chrono::microseconds semantic_time{0};
    std::chrono::microseconds bytecode_time{0};
    std::chrono::microseconds total_time{0};

    size_t token_count = 0;
    size_t ast_node_count = 0;
    size_t bytecode_size = 0;

    bool cache_hit = false;
};

/**
 * Compilation result
 */
class CompilationResultV2 {
public:
    CompilationResultV2() = default;

    bool success() const { return errors_.empty() && !bytecode_.empty(); }
    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    const CompilationStats& stats() const { return stats_; }

    // For cache - tables involved in query (for invalidation)
    const std::unordered_set<core::ID, core::IDHash>& involvedTables() const {
        return involved_tables_;
    }

    const std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>>& dependencies() const {
        return dependencies_;
    }

    // Internal setters (used by QueryCompilerV2)
    void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }
    void addError(const std::string& err) { errors_.push_back(err); }
    void addWarning(const std::string& warn) { warnings_.push_back(warn); }
    void setStats(const CompilationStats& s) { stats_ = s; }
    void addInvolvedTable(const core::ID& table_id) { involved_tables_.insert(table_id); }
    void addDependency(const std::pair<core::ID, core::CatalogManager::ObjectType>& dep) {
        dependencies_.push_back(dep);
    }

private:
    std::vector<uint8_t> bytecode_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    CompilationStats stats_;
    std::unordered_set<core::ID, core::IDHash> involved_tables_;
    std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>> dependencies_;
};

/**
 * Query Compiler V2 - Main compilation interface
 *
 * Usage:
 *   QueryCompilerV2 compiler(database);
 *   auto result = compiler.compile("SELECT * FROM users");
 *   if (result.success()) {
 *       executor.execute(result.bytecode());
 *   }
 */
class QueryCompilerV2 {
public:
    /**
     * Create a compiler for the given database
     *
     * @param db Database instance (for catalog lookups)
     */
    explicit QueryCompilerV2(core::Database* db);
    ~QueryCompilerV2();

    // Non-copyable
    QueryCompilerV2(const QueryCompilerV2&) = delete;
    QueryCompilerV2& operator=(const QueryCompilerV2&) = delete;

    /**
     * Compile SQL to bytecode
     *
     * @param sql SQL statement to compile
     * @return Compilation result with bytecode or errors
     */
    CompilationResultV2 compile(const std::string& sql);

    /**
     * Compile SQL with caching
     *
     * First checks the query cache. If found, returns cached bytecode.
     * Otherwise compiles and caches the result.
     *
     * @param sql SQL statement to compile
     * @param use_cache Whether to use query cache (default: true)
     * @return Compilation result
     */
    CompilationResultV2 compileWithCache(const std::string& sql, bool use_cache = true);

    /**
     * Set the current schema for name resolution
     *
     * @param schema_id Schema UUID
     */
    void setCurrentSchema(const core::ID& schema_id) { current_schema_ = schema_id; }

    /**
     * Set the schema search path for name resolution
     *
     * @param path Schema UUIDs in search order
     */
    void setSearchPath(const std::vector<core::ID>& path) { search_path_ = path; }

    /**
     * Enable/disable optimization passes
     *
     * @param enabled True to enable optimizations (default: true)
     */
    void setOptimizationsEnabled(bool enabled) { optimizations_enabled_ = enabled; }

    /**
     * Enable/disable detailed statistics collection
     *
     * @param enabled True to collect detailed stats (adds overhead)
     */
    void setStatsEnabled(bool enabled) { stats_enabled_ = enabled; }

    /**
     * Get total compilation statistics (across all compilations)
     */
    const CompilationStats& totalStats() const { return total_stats_; }

    /**
     * Reset total statistics
     */
    void resetStats() { total_stats_ = CompilationStats{}; }

private:
    core::Database* db_;
    core::CatalogManager* catalog_;
    core::ID default_schema_;
    core::ID current_schema_;
    std::vector<core::ID> search_path_;
    bool optimizations_enabled_ = true;
    bool stats_enabled_ = false;

    CompilationStats total_stats_;

    // Internal compilation (no caching)
    CompilationResultV2 compileInternal(const std::string& sql);

    // Extract table IDs from resolved AST for cache invalidation
    void extractInvolvedTables(parser::v2::ResolvedStatement* stmt,
                               std::unordered_set<core::ID, core::IDHash>& tables);

    // Extract dependency object IDs (tables, functions, sequences, schemas where available)
    void collectDependencies(parser::v2::ResolvedStatement* stmt,
                             parser::v2::StringPool* pool,
                             std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>>& deps);
};

/**
 * Prepared statement handle for V2 compiler
 *
 * Stores compiled bytecode for repeated execution with different parameters.
 * (Phase 9 - basic implementation, Phase 10 will add parameter binding)
 */
class PreparedStatementV2 {
public:
    PreparedStatementV2() = default;

    bool isValid() const { return !bytecode_.empty(); }
    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    const std::string& sql() const { return sql_; }

    // Internal
    void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }
    void setSql(const std::string& sql) { sql_ = sql; }

private:
    std::string sql_;
    std::vector<uint8_t> bytecode_;
};

} // namespace sblr
} // namespace scratchbird
