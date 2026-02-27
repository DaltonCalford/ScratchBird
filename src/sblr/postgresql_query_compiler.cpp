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
 * PostgreSQL Query Compiler Implementation
 *
 * Compiles PostgreSQL SQL to canonical V3 SBLR container bytecode.
 * The PostgreSQL parser produces a V3 AST which is emitted to container form.
 *
 * Default schema: supplied by adapter/session and rooted in selected emulated DB schema.
 */

#include "scratchbird/sblr/postgresql_query_compiler.h"
#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/v3_container.h"
#include <chrono>

namespace scratchbird {
namespace sblr {

// Use explicit namespace prefix
namespace pg = parser::postgresql;

PostgreSQLQueryCompiler::PostgreSQLQueryCompiler(core::Database* db)
    : db_(db)
    , catalog_(db ? db->catalog_manager() : nullptr)
{
    // Default schema/search path are intentionally left unset here.
    // The adapter/session must set them to the selected emulated DB schema root.
}

PostgreSQLQueryCompiler::~PostgreSQLQueryCompiler() = default;

PostgreSQLCompilationResult PostgreSQLQueryCompiler::compile(const std::string& sql) {
    return compileInternal(sql);
}

PostgreSQLCompilationResult PostgreSQLQueryCompiler::compileInternal(const std::string& sql) {
    PostgreSQLCompilationResult result;
    PostgreSQLCompilationStats stats;

    auto total_start = std::chrono::steady_clock::now();

    // =========================================================================
    // Phase 1: Parse SQL into V3 AST
    // =========================================================================

    auto parse_start = std::chrono::steady_clock::now();

    // Create PostgreSQL parser
    pg::Parser parser(sql, db_, default_schema_);
    pg::ParseResult parse_result = parser.parseStatement();

    auto parse_end = std::chrono::steady_clock::now();
    stats.parser_time = std::chrono::duration_cast<std::chrono::microseconds>(
        parse_end - parse_start);

    if (!parse_result.success()) {
        for (const auto& err : parse_result.errors()) {
            result.addError("Parse error at line " + std::to_string(err.location.line) +
                          ", column " + std::to_string(err.location.column) + ": " + err.message);
        }
        return result;
    }

    // =========================================================================
    // Phase 2: Emit canonical V3 SBLR container
    // =========================================================================

    if (parse_result.statement() != nullptr) {
        parser::v3::V3Emitter emitter(parse_result.stringPool());
        sblr::v3::Container container;
        std::string emit_err;
        if (!emitter.emitStatementToContainer(parse_result.statement(), container, emit_err)) {
            result.addError("V3 emit failed: " + emit_err);
            return result;
        }
        container.metadata.module_name = "postgresql_emulation";

        std::vector<uint8_t> encoded;
        std::string encode_err;
        if (!sblr::v3::encodeContainer(container, encoded, encode_err)) {
            result.addError("V3 container encode failed: " + encode_err);
            return result;
        }
        result.setBytecode(std::move(encoded));
    } else {
        result.addError("V3 AST not available for PostgreSQL statement");
        return result;
    }
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
