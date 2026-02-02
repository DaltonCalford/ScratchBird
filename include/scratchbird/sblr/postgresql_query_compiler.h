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
 * PostgreSQL Query Compiler
 *
 * Compiles PostgreSQL SQL to SBLR bytecode using:
 * - PostgreSQL Lexer (PostgreSQL SQL tokenization)
 * - PostgreSQL Parser (PostgreSQL SQL → AST v2)
 * - Semantic Analyzer V2 (shared - name resolution, type checking)
 * - Bytecode Generator V2 (shared - AST v2 → SBLR bytecode)
 *
 * This compiler allows PostgreSQL SQL syntax to be executed on the
 * ScratchBird engine, enabling PostgreSQL client emulation.
 *
 * Default schema: emulation.postgresql.localhost.databases.default
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace scratchbird {
namespace sblr {

/**
 * PostgreSQL compilation statistics
 */
struct PostgreSQLCompilationStats {
    std::chrono::microseconds lexer_time{0};
    std::chrono::microseconds parser_time{0};
    std::chrono::microseconds semantic_time{0};
    std::chrono::microseconds bytecode_time{0};
    std::chrono::microseconds total_time{0};

    size_t token_count = 0;
    size_t ast_node_count = 0;
    size_t bytecode_size = 0;
};

/**
 * PostgreSQL compilation result
 */
class PostgreSQLCompilationResult {
public:
    PostgreSQLCompilationResult() = default;

    bool success() const { return errors_.empty() && !bytecode_.empty(); }
    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    const PostgreSQLCompilationStats& stats() const { return stats_; }

    // Internal setters
    void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }
    void addError(const std::string& err) { errors_.push_back(err); }
    void addWarning(const std::string& warn) { warnings_.push_back(warn); }
    void setStats(const PostgreSQLCompilationStats& s) { stats_ = s; }

private:
    std::vector<uint8_t> bytecode_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    PostgreSQLCompilationStats stats_;
};

/**
 * PostgreSQL Query Compiler
 *
 * Usage:
 *   PostgreSQLQueryCompiler compiler(database);
 *   auto result = compiler.compile("SELECT * FROM users LIMIT 10");
 *   if (result.success()) {
 *       executor.execute(result.bytecode());
 *   }
 */
class PostgreSQLQueryCompiler {
public:
    /**
     * Create a compiler for the given database
     *
     * @param db Database instance (for catalog lookups)
     */
    explicit PostgreSQLQueryCompiler(core::Database* db);
    ~PostgreSQLQueryCompiler();

    // Non-copyable
    PostgreSQLQueryCompiler(const PostgreSQLQueryCompiler&) = delete;
    PostgreSQLQueryCompiler& operator=(const PostgreSQLQueryCompiler&) = delete;

    /**
     * Compile PostgreSQL SQL to bytecode
     *
     * @param sql PostgreSQL SQL statement
     * @return Compilation result with bytecode or errors
     */
    PostgreSQLCompilationResult compile(const std::string& sql);

    /**
     * Get/set current schema (for name resolution)
     * Default: emulation.postgresql.localhost.databases.default
     */
    const core::ID& currentSchema() const { return current_schema_; }
    void setCurrentSchema(const core::ID& schema_id) { current_schema_ = schema_id; }

    /**
     * Get/set parser default schema path (dot-separated)
     */
    const std::string& defaultSchema() const { return default_schema_; }
    void setDefaultSchema(const std::string& schema) { default_schema_ = schema; }

    /**
     * Get/set search path
     */
    const std::vector<std::string>& searchPath() const { return search_path_; }
    void setSearchPath(const std::vector<std::string>& path) { search_path_ = path; }

    /**
     * Enable/disable optimizations
     */
    bool optimizationsEnabled() const { return optimizations_enabled_; }
    void setOptimizationsEnabled(bool enabled) { optimizations_enabled_ = enabled; }

    /**
     * Enable/disable statistics collection
     */
    bool statsEnabled() const { return stats_enabled_; }
    void setStatsEnabled(bool enabled) { stats_enabled_ = enabled; }

private:
    core::Database* db_ = nullptr;
    core::CatalogManager* catalog_ = nullptr;
    core::ID current_schema_;
    std::string default_schema_ = "emulation.postgresql.localhost.databases.default";
    std::vector<std::string> search_path_ = {"public"};
    bool optimizations_enabled_ = true;
    bool stats_enabled_ = false;

    PostgreSQLCompilationResult compileInternal(const std::string& sql);
};

} // namespace sblr
} // namespace scratchbird
