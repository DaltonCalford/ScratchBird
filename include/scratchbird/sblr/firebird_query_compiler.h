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
 * Firebird Query Compiler
 *
 * Compiles Firebird SQL to SBLR bytecode using:
 * - Firebird Lexer (Firebird SQL tokenization)
 * - Firebird Parser (Firebird SQL → AST v2)
 * - Semantic Analyzer V2 (shared - name resolution, type checking)
 * - Bytecode Generator V2 (shared - AST v2 → SBLR bytecode)
 *
 * This compiler allows Firebird SQL syntax to be executed on the
 * ScratchBird engine, enabling Firebird client emulation.
 */

#include "scratchbird/parser/firebird/firebird_parser.h"
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
 * Firebird compilation statistics
 */
struct FirebirdCompilationStats {
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
 * Firebird compilation result
 */
class FirebirdCompilationResult {
public:
    FirebirdCompilationResult() = default;

    bool success() const { return errors_.empty() && !bytecode_.empty(); }
    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    const FirebirdCompilationStats& stats() const { return stats_; }

    // Internal setters
    void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }
    void addError(const std::string& err) { errors_.push_back(err); }
    void addWarning(const std::string& warn) { warnings_.push_back(warn); }
    void setStats(const FirebirdCompilationStats& s) { stats_ = s; }

private:
    std::vector<uint8_t> bytecode_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    FirebirdCompilationStats stats_;
};

/**
 * Firebird Query Compiler
 *
 * Usage:
 *   FirebirdQueryCompiler compiler(database);
 *   auto result = compiler.compile("SELECT FIRST 10 * FROM users");
 *   if (result.success()) {
 *       executor.execute(result.bytecode());
 *   }
 */
class FirebirdQueryCompiler {
public:
    /**
     * Create a compiler for the given database
     *
     * @param db Database instance (for catalog lookups)
     */
    explicit FirebirdQueryCompiler(core::Database* db);
    ~FirebirdQueryCompiler();

    // Non-copyable
    FirebirdQueryCompiler(const FirebirdQueryCompiler&) = delete;
    FirebirdQueryCompiler& operator=(const FirebirdQueryCompiler&) = delete;

    /**
     * Compile Firebird SQL to bytecode
     *
     * @param sql Firebird SQL statement
     * @return Compilation result with bytecode or errors
     */
    FirebirdCompilationResult compile(const std::string& sql);

    /**
     * Get/set SQL dialect (1, 2, or 3)
     */
    parser::firebird::SQLDialect dialect() const { return dialect_; }
    void setDialect(parser::firebird::SQLDialect dialect) { dialect_ = dialect; }

    /**
     * Get/set current schema (for name resolution)
     */
    const core::ID& currentSchema() const { return current_schema_; }
    void setCurrentSchema(const core::ID& schema_id) { current_schema_ = schema_id; }

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
    parser::firebird::SQLDialect dialect_ = parser::firebird::SQLDialect::DIALECT_3;
    bool optimizations_enabled_ = true;
    bool stats_enabled_ = false;

    FirebirdCompilationResult compileInternal(const std::string& sql);
};

} // namespace sblr
} // namespace scratchbird
