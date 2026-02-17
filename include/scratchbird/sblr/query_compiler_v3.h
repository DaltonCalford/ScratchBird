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
 * Canonical native query compiler (V3 backend).
 *
 * Compiles SQL using parser v3 + V3 emitter and encodes SBLR v3 containers.
 */

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/types.h"
#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_payloads.h"

namespace scratchbird::sblr {

class QueryCompilerV3 {
public:
    struct CompilationStats {
        size_t bytecode_size = 0;
        std::chrono::microseconds parser_time{0};
    };

    struct CompileResult {
        bool success() const { return errors_.empty() && !bytecode_.empty(); }
        const std::vector<uint8_t>& bytecode() const { return bytecode_; }
        const std::vector<std::string>& errors() const { return errors_; }
        const std::vector<std::string>& warnings() const { return warnings_; }
        const CompilationStats& stats() const { return stats_; }

        void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }
        void addError(const std::string& err) { errors_.push_back(err); }
        void addWarning(const std::string& warn) { warnings_.push_back(warn); }
        void setStats(const CompilationStats& stats) { stats_ = stats; }

    private:
        std::vector<uint8_t> bytecode_;
        std::vector<std::string> errors_;
        std::vector<std::string> warnings_;
        CompilationStats stats_{};
    };

    struct TraceDigest {
        std::string normalized_sql;
        std::string sql_hash;
        std::string ast_hash;
        std::string sblr_hash;
        std::string root_opcode_symbol;
    };

    struct TraceResult {
        bool success() const { return errors_.empty() && digest_.has_value(); }
        const TraceDigest& digest() const { return *digest_; }
        const std::vector<std::string>& errors() const { return errors_; }
        const std::vector<std::string>& warnings() const { return warnings_; }

        void setDigest(TraceDigest digest) { digest_ = std::move(digest); }
        void addError(const std::string& err) { errors_.push_back(err); }
        void addWarning(const std::string& warn) { warnings_.push_back(warn); }

    private:
        std::optional<TraceDigest> digest_;
        std::vector<std::string> errors_;
        std::vector<std::string> warnings_;
    };

    explicit QueryCompilerV3(core::Database* db = nullptr)
        : db_(db) {}

    void setCurrentSchema(const core::ID& schema_id) { current_schema_ = schema_id; }
    void setCurrentSchemaId(const core::ID& schema_id) { setCurrentSchema(schema_id); }
    void setStatsEnabled(bool enabled) { stats_enabled_ = enabled; }
    void setOptimizationsEnabled(bool enabled) { optimizations_enabled_ = enabled; }

    CompileResult compile(const std::string& sql) {
        CompileResult result;
        if (db_ == nullptr) {
            result.addError("Database context is required for QueryCompilerV3");
            return result;
        }

        auto parser_start = std::chrono::steady_clock::now();
        parser::v3::Parser parser(sql);
        parser::v3::ParseResult parse_result = parser.parseStatement();
        auto parser_end = std::chrono::steady_clock::now();
        if (!parse_result.success()) {
            for (const auto& err : parse_result.errors()) {
                result.addError("Parse error at line " + std::to_string(err.span.start.line) +
                                ", column " + std::to_string(err.span.start.column) + ": " + err.message);
            }
            return result;
        }

        parser::v3::V3Emitter emitter(parser.stringPool());
        sblr::v3::Container container;
        std::string emit_err;
        if (!emitter.emitStatementToContainer(parse_result.statement(), container, emit_err)) {
            result.addError(emit_err.empty() ? "V3 emit failed" : emit_err);
            return result;
        }
        container.metadata.module_name = "scratchbird_native";

        std::vector<uint8_t> encoded;
        std::string encode_err;
        if (!sblr::v3::encodeContainer(container, encoded, encode_err)) {
            result.addError(encode_err.empty() ? "V3 container encode failed" : encode_err);
            return result;
        }
        result.setBytecode(std::move(encoded));
        if (stats_enabled_) {
            CompilationStats stats;
            stats.bytecode_size = result.bytecode().size();
            stats.parser_time = std::chrono::duration_cast<std::chrono::microseconds>(
                parser_end - parser_start);
            result.setStats(stats);
        }
        return result;
    }

    TraceResult compileTrace(const std::string& sql) {
        TraceResult trace;
        if (db_ == nullptr) {
            trace.addError("Database context is required for QueryCompilerV3");
            return trace;
        }

        parser::v3::Parser parser(sql);
        parser::v3::ParseResult parse_result = parser.parseStatement();
        if (!parse_result.success()) {
            for (const auto& err : parse_result.errors()) {
                trace.addError("Parse error at line " + std::to_string(err.span.start.line) +
                               ", column " + std::to_string(err.span.start.column) + ": " + err.message);
            }
            return trace;
        }

        parser::v3::V3Emitter emitter(parser.stringPool());
        sblr::v3::Container container;
        std::string emit_err;
        if (!emitter.emitStatementToContainer(parse_result.statement(), container, emit_err)) {
            trace.addError(emit_err.empty() ? "V3 emit failed" : emit_err);
            return trace;
        }
        container.metadata.module_name = "scratchbird_native";

        std::vector<uint8_t> encoded;
        std::string encode_err;
        if (!sblr::v3::encodeContainer(container, encoded, encode_err)) {
            trace.addError(encode_err.empty() ? "V3 container encode failed" : encode_err);
            return trace;
        }

        sblr::v3::Container decoded;
        std::string decode_err;
        if (!sblr::v3::decodeContainer(encoded.data(), encoded.size(), decoded, decode_err)) {
            trace.addError(decode_err.empty() ? "V3 container decode failed" : decode_err);
            return trace;
        }

        size_t offset = 0;
        sblr::v3::DecodeError instruction_err;
        sblr::v3::Instruction version_inst;
        if (!sblr::v3::decodeInstructionWithSchema(decoded.bytecode_stream.data(),
                                                   decoded.bytecode_stream.size(),
                                                   offset,
                                                   version_inst,
                                                   instruction_err)) {
            trace.addError(instruction_err.message.empty()
                               ? "V3 instruction decode failed for version opcode"
                               : instruction_err.message);
            return trace;
        }
        sblr::v3::Instruction root_inst;
        if (!sblr::v3::decodeInstructionWithSchema(decoded.bytecode_stream.data(),
                                                   decoded.bytecode_stream.size(),
                                                   offset,
                                                   root_inst,
                                                   instruction_err)) {
            trace.addError(instruction_err.message.empty()
                               ? "V3 instruction decode failed for root opcode"
                               : instruction_err.message);
            return trace;
        }

        sblr::v3::Buffer root_bytes;
        if (!sblr::v3::encodeInstructionWithSchema(root_inst, root_bytes, instruction_err)) {
            trace.addError(instruction_err.message.empty()
                               ? "V3 root instruction encode failed"
                               : instruction_err.message);
            return trace;
        }

        const std::string normalized_sql = normalizeSqlForTrace(sql);
        const std::string statement_kind = parser::v3::astKindToString(parse_result.statement()->kind());
        const std::string root_symbol = sblr::v3::canonicalOpcodeSymbolForOpcode(root_inst.opcode);
        const std::string root_instruction_hash = hashBytesHex(root_bytes);

        std::string ast_fingerprint;
        ast_fingerprint.reserve(statement_kind.size() + root_symbol.size() + root_instruction_hash.size() + 2);
        ast_fingerprint.append(statement_kind);
        ast_fingerprint.push_back('|');
        ast_fingerprint.append(root_symbol);
        ast_fingerprint.push_back('|');
        ast_fingerprint.append(root_instruction_hash);

        TraceDigest digest;
        digest.normalized_sql = normalized_sql;
        digest.sql_hash = hashStringHex(normalized_sql);
        digest.ast_hash = hashStringHex(ast_fingerprint);
        digest.sblr_hash = hashBytesHex(encoded);
        digest.root_opcode_symbol = root_symbol;
        trace.setDigest(std::move(digest));
        return trace;
    }

private:
    static uint64_t fnv1a64(const uint8_t* data, size_t len) {
        uint64_t hash = 1469598103934665603ULL;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static std::string toHex64(uint64_t value) {
        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::setw(16) << value;
        return out.str();
    }

    static std::string hashStringHex(const std::string& text) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text.data());
        return toHex64(fnv1a64(bytes, text.size()));
    }

    static std::string hashBytesHex(const std::vector<uint8_t>& bytes) {
        return toHex64(fnv1a64(bytes.data(), bytes.size()));
    }

    static std::string normalizeSqlForTrace(const std::string& sql) {
        std::string out;
        out.reserve(sql.size());

        bool prev_space = true;
        bool in_single_quote = false;
        bool in_double_quote = false;

        for (char c : sql) {
            if (c == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
                out.push_back(c);
                prev_space = false;
                continue;
            }
            if (c == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
                out.push_back(c);
                prev_space = false;
                continue;
            }

            if (!in_single_quote && !in_double_quote) {
                if (std::isspace(static_cast<unsigned char>(c)) != 0) {
                    if (!prev_space) {
                        out.push_back(' ');
                        prev_space = true;
                    }
                    continue;
                }
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                prev_space = false;
            } else {
                out.push_back(c);
                prev_space = false;
            }
        }

        if (!out.empty() && out.back() == ' ') {
            out.pop_back();
        }
        return out;
    }

    core::Database* db_ = nullptr;
    core::ID current_schema_{};
    bool stats_enabled_ = false;
    bool optimizations_enabled_ = true;
};

using CompilationResultV3 = QueryCompilerV3::CompileResult;

}  // namespace scratchbird::sblr
