/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/sblr/ast_sblr_lowerer.h"
#include "scratchbird/sblr/query_compiler_v3_optimizer_support.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_codec.h"

namespace scratchbird {
namespace sblr {

namespace mysql = parser::mysql;

MySQLQueryCompiler::MySQLQueryCompiler(core::Database* db)
    : db_(db)
    , catalog_(db ? db->catalog_manager() : nullptr)
    , default_schema_("")
{
}

MySQLQueryCompiler::~MySQLQueryCompiler() = default;

MySQLCompilationResult MySQLQueryCompiler::compile(const std::string& sql) {
    return compileInternal(sql);
}

MySQLCompilationResult MySQLQueryCompiler::compileInternal(const std::string& sql) {
    MySQLCompilationResult result;
    MySQLCompilationStats stats;

    auto total_start = std::chrono::steady_clock::now();

    // Parse to V3 AST, then emit canonical V3 container bytecode.
    auto parse_start = std::chrono::steady_clock::now();
    mysql::Parser parser(sql, db_, default_schema_);
    parser.setCompatibilityMode(compat_mode_);
    mysql::ParseResult parse_result = parser.parseStatement();
    auto parse_end = std::chrono::steady_clock::now();

    stats.parser_time = std::chrono::duration_cast<std::chrono::microseconds>(
        parse_end - parse_start);

    if (!parse_result.success()) {
        for (const auto& err : parse_result.errors()) {
            result.addError("Parse error at line " + std::to_string(err.location.line) +
                            ", column " + std::to_string(err.location.column) +
                            ": " + err.message);
        }
        return result;
    }

    if (parse_result.statement() != nullptr) {
        parser::v3::AstSblrLowerer emitter(parse_result.stringPool());
        sblr::v3::Container container;
        std::string emit_err;
        if (!emitter.emitStatementToContainer(parse_result.statement(), container, emit_err)) {
            result.addError("V3 emit failed: " + emit_err);
            return result;
        }
        container.metadata.module_name = "mysql_emulation";

        if (db_ != nullptr) {
            auto finalized = detail::finalizeQueryCompilerV3Compilation(
                db_,
                sql,
                parse_result.statement(),
                parse_result.stringPool(),
                core::ID{},
                true,
                container);
            for (const auto& warning : finalized.warnings) {
                result.addWarning(warning);
            }
            for (const auto& error : finalized.errors) {
                result.addError(error);
            }
            if (!finalized.success) {
                return result;
            }
            result.setBytecode(std::move(finalized.bytecode));
        } else {
            std::string encode_error;
            std::vector<uint8_t> raw_bytecode;
            if (!sblr::v3::encodeContainer(container, raw_bytecode, encode_error)) {
                result.addError("V3 raw encode failed: " + encode_error);
                return result;
            }
            result.setBytecode(std::move(raw_bytecode));
        }
    } else {
        // No-op input chunk (e.g. comment-only statement segment).
        sblr::v3::Container container;
        container.header.version_major = 3;
        container.header.version_minor = 0;
        container.header.version_patch = 0;
        container.header.flags = 0;
        container.metadata.module_name = "mysql_emulation";
        container.metadata.module_version = "v3";
        container.metadata.dialect_id = 1;
        container.metadata.target_platform = 0;
        container.metadata.build_id.clear();
        container.metadata.source_hash.clear();

        sblr::v3::Buffer bytecode_stream;
        sblr::v3::DecodeError derr;

        sblr::v3::Instruction version_inst;
        version_inst.opcode = static_cast<uint16_t>(sblr::v3::Opcode::SBLR3_VERSION);
        version_inst.flags = 0;
        sblr::v3::Value::Bytes version_payload(6, 0);
        version_payload[0] = 3;
        version_inst.payload = sblr::v3::Value(std::move(version_payload));
        if (!sblr::v3::encodeInstructionWithSchema(version_inst, bytecode_stream, derr)) {
            result.addError("V3 no-op encode failed: " + derr.message);
            return result;
        }

        sblr::v3::Instruction end_inst;
        end_inst.opcode = static_cast<uint16_t>(sblr::v3::Opcode::SBLR3_END);
        end_inst.flags = 0;
        end_inst.payload = sblr::v3::Value(sblr::v3::Value::Bytes{});
        if (!sblr::v3::encodeInstructionWithSchema(end_inst, bytecode_stream, derr)) {
            result.addError("V3 no-op encode failed: " + derr.message);
            return result;
        }

        container.bytecode_stream = std::move(bytecode_stream);

        if (db_ != nullptr) {
            auto finalized = detail::finalizeQueryCompilerV3Compilation(
                db_,
                sql,
                nullptr,
                parse_result.stringPool(),
                core::ID{},
                true,
                container);
            for (const auto& warning : finalized.warnings) {
                result.addWarning(warning);
            }
            for (const auto& error : finalized.errors) {
                result.addError(error);
            }
            if (!finalized.success) {
                return result;
            }
            result.setBytecode(std::move(finalized.bytecode));
        } else {
            std::string encode_error;
            std::vector<uint8_t> raw_bytecode;
            if (!sblr::v3::encodeContainer(container, raw_bytecode, encode_error)) {
                result.addError("V3 no-op encode failed: " + encode_error);
                return result;
            }
            result.setBytecode(std::move(raw_bytecode));
        }
    }
    for (const auto& warn : parse_result.warnings()) {
        result.addWarning(warn);
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
