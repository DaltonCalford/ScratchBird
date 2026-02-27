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
#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/v3_container.h"
#include <chrono>

namespace scratchbird {
namespace sblr {

// Use explicit namespace prefixes to avoid ambiguity
namespace fb = parser::firebird;


FirebirdQueryCompiler::FirebirdQueryCompiler(core::Database* db)
    : db_(db)
    , catalog_(db ? db->catalog_manager() : nullptr)
{
    // Current schema is supplied by the Firebird adapter/session.
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
    // Phase 2: Emit V3 SBLR container from parser AST
    // =========================================================================

    parser::v3::V3Emitter emitter(parser.stringPool());
    sblr::v3::Container container;
    std::string emit_err;

    if (!emitter.emitStatementToContainer(parse_result.statement.get(), container, emit_err)) {
        result.addError("V3 emit failed: " + emit_err);
        return result;
    }

    // Annotate module metadata for Firebird emulation (dialect id reserved by spec)
    container.metadata.module_name = "firebird_emulation";

    std::vector<uint8_t> encoded;
    std::string encode_err;
    if (!sblr::v3::encodeContainer(container, encoded, encode_err)) {
        result.addError("V3 container encode failed: " + encode_err);
        return result;
    }

    result.setBytecode(std::move(encoded));
    stats.bytecode_size = result.bytecode().size();

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
