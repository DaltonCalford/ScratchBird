/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Firebird Query Compiler Implementation
 *
 * Compiles Firebird SQL to SBLR bytecode using the Firebird parser
 * and shared semantic analyzer / bytecode generator.
 */

#include "scratchbird/sblr/firebird_query_compiler.h"
#include <chrono>

namespace scratchbird {
namespace sblr {

// Use explicit namespace prefixes to avoid ambiguity
namespace fb = parser::firebird;

// Import v2 types used from parser namespace
using parser::v2::SemanticAnalyzerV2;
using parser::v2::SemanticResult;
using parser::v2::BytecodeGeneratorV2;
using parser::v2::BytecodeResultV2;

FirebirdQueryCompiler::FirebirdQueryCompiler(core::Database* db)
    : db_(db)
    , catalog_(db ? db->catalog_manager() : nullptr)
{
    // Initialize with public schema if available
    if (catalog_) {
        core::CatalogManager::SchemaInfo public_schema_info;
        core::ErrorContext ctx;
        if (catalog_->getSchema("users.public", public_schema_info, &ctx) == core::Status::OK ||
            catalog_->getSchema("public", public_schema_info, &ctx) == core::Status::OK) {
            current_schema_ = public_schema_info.schema_id;
        }
    }
}

FirebirdQueryCompiler::~FirebirdQueryCompiler() = default;

FirebirdCompilationResult FirebirdQueryCompiler::compile(const std::string& sql) {
    return compileInternal(sql);
}

FirebirdCompilationResult FirebirdQueryCompiler::compileInternal(const std::string& sql) {
    FirebirdCompilationResult result;
    FirebirdCompilationStats stats;

    auto total_start = std::chrono::steady_clock::now();

    // =========================================================================
    // Phase 1: Lexing and Parsing with Firebird Parser
    // =========================================================================

    auto parse_start = std::chrono::steady_clock::now();

    // Create Firebird parser
    fb::Parser parser(sql, dialect_);
    fb::ParseResult parse_result = parser.parseStatement();

    auto parse_end = std::chrono::steady_clock::now();
    stats.parser_time = std::chrono::duration_cast<std::chrono::microseconds>(
        parse_end - parse_start);

    if (!parse_result.success) {
        for (const auto& err : parse_result.errors) {
            result.addError("Parse error at line " + std::to_string(err.location.line) +
                          ", column " + std::to_string(err.location.column) + ": " + err.message);
        }
        return result;
    }

    // =========================================================================
    // Phase 2: Semantic Analysis (shared with ScratchBird parser)
    // =========================================================================

    if (!catalog_) {
        result.addError("Catalog manager not available");
        return result;
    }

    auto semantic_start = std::chrono::steady_clock::now();

    SemanticAnalyzerV2 analyzer(*catalog_, parser.stringPool());
    analyzer.setCurrentSchema(current_schema_);

    SemanticResult sem_result = analyzer.analyze(parse_result.statement.get());

    auto semantic_end = std::chrono::steady_clock::now();
    stats.semantic_time = std::chrono::duration_cast<std::chrono::microseconds>(
        semantic_end - semantic_start);

    if (!sem_result.success()) {
        for (const auto& err : sem_result.errors()) {
            result.addError("Semantic error at line " + std::to_string(err.span.start.line) +
                          ", column " + std::to_string(err.span.start.column) + ": " + err.message);
        }
        for (const auto& warn : sem_result.warnings()) {
            result.addWarning("Warning at line " + std::to_string(warn.span.start.line) +
                            ", column " + std::to_string(warn.span.start.column) + ": " + warn.message);
        }
        return result;
    }

    // Copy warnings even on success
    for (const auto& warn : sem_result.warnings()) {
        result.addWarning("Warning at line " + std::to_string(warn.span.start.line) +
                        ", column " + std::to_string(warn.span.start.column) + ": " + warn.message);
    }

    // =========================================================================
    // Phase 3: Bytecode Generation (shared with ScratchBird parser)
    // =========================================================================

    auto bytecode_start = std::chrono::steady_clock::now();

    BytecodeGeneratorV2 generator(parser.stringPool());
    generator.setOptimizationsEnabled(optimizations_enabled_);
    generator.setSourceSql(sql);

    BytecodeResultV2 bc_result = generator.generate(sem_result.statement());

    auto bytecode_end = std::chrono::steady_clock::now();
    stats.bytecode_time = std::chrono::duration_cast<std::chrono::microseconds>(
        bytecode_end - bytecode_start);

    if (!bc_result.success()) {
        for (const auto& err : bc_result.errors()) {
            result.addError("Bytecode generation error: " + err);
        }
        for (const auto& warn : bc_result.warnings()) {
            result.addWarning("Bytecode warning: " + warn);
        }
        return result;
    }

    // Copy warnings
    for (const auto& warn : bc_result.warnings()) {
        result.addWarning("Bytecode warning: " + warn);
    }

    // =========================================================================
    // Success - set bytecode and stats
    // =========================================================================

    result.setBytecode(bc_result.bytecode());
    stats.bytecode_size = bc_result.bytecode().size();

    auto total_end = std::chrono::steady_clock::now();
    stats.total_time = std::chrono::duration_cast<std::chrono::microseconds>(
        total_end - total_start);

    if (stats_enabled_) {
        result.setStats(stats);
    }

    return result;
}

} // namespace sblr
} // namespace scratchbird
