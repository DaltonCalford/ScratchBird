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
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/types.h"
#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/native_sql_renderer.h"
#include "scratchbird/sblr/query_compiler_v3_optimizer_support.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/udr/language_udr_sql_render_endpoint.h"

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
        const detail::QueryCompilerV3PlanProfile& planProfile() const { return plan_profile_; }

        void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }
        void addError(const std::string& err) { errors_.push_back(err); }
        void addWarning(const std::string& warn) { warnings_.push_back(warn); }
        void setStats(const CompilationStats& stats) { stats_ = stats; }
        void setPlanProfile(detail::QueryCompilerV3PlanProfile profile)
        {
            plan_profile_ = std::move(profile);
        }

    private:
        std::vector<uint8_t> bytecode_;
        std::vector<std::string> errors_;
        std::vector<std::string> warnings_;
        CompilationStats stats_{};
        detail::QueryCompilerV3PlanProfile plan_profile_{};
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
        const std::string& diagnostic_sql_context() const { return diagnostic_sql_context_; }

        void setDigest(TraceDigest digest) { digest_ = std::move(digest); }
        void addError(const std::string& err) { errors_.push_back(err); }
        void addWarning(const std::string& warn) { warnings_.push_back(warn); }
        void setDiagnosticSqlContext(std::string value) { diagnostic_sql_context_ = std::move(value); }

    private:
        std::optional<TraceDigest> digest_;
        std::vector<std::string> errors_;
        std::vector<std::string> warnings_;
        std::string diagnostic_sql_context_;
    };

    explicit QueryCompilerV3(core::Database* db = nullptr)
        : db_(db) {}

    void setCurrentSchema(const core::ID& schema_id) { current_schema_ = schema_id; }
    void setCurrentSchemaId(const core::ID& schema_id) { setCurrentSchema(schema_id); }
    void setStatsEnabled(bool enabled) { stats_enabled_ = enabled; }
    void setOptimizationsEnabled(bool enabled) { optimizations_enabled_ = enabled; }

    static auto planCacheStats() -> optimizer::VNextPlanCacheStats
    {
        return detail::queryCompilerV3PlanCacheStats();
    }

    static auto resetPlanCacheStats() -> void
    {
        detail::resetQueryCompilerV3PlanCacheStats();
    }

    static auto invalidateAllPlanCache() -> uint64_t
    {
        return detail::invalidateAllQueryCompilerV3PlanCache();
    }

    CompileResult compileWithParameters(
        const std::string& sql,
        const optimizer::ParameterBindings& parameter_bindings,
        detail::QueryCompilerV3PlanProfileMode plan_profile_mode =
            detail::QueryCompilerV3PlanProfileMode::CUSTOM) {
        return compileInternal(sql, &parameter_bindings, plan_profile_mode);
    }

    CompileResult compile(const std::string& sql) {
        return compileInternal(sql, nullptr, detail::QueryCompilerV3PlanProfileMode::GENERIC);
    }

    CompileResult compileInternal(
        const std::string& sql,
        const optimizer::ParameterBindings* parameter_bindings,
        detail::QueryCompilerV3PlanProfileMode plan_profile_mode) {
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

        auto finalized = detail::finalizeQueryCompilerV3Compilation(db_,
                                                                    sql,
                                                                    parse_result.statement(),
                                                                    parser.stringPool(),
                                                                    current_schema_,
                                                                    optimizations_enabled_,
                                                                    container,
                                                                    parameter_bindings,
                                                                    plan_profile_mode);
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
        result.setPlanProfile(finalized.plan_profile);
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
        parser::v3::Parser parser(sql);
        trace.setDiagnosticSqlContext(normalizeSqlForTrace(sql, nullptr, parser.stringPool()));

        if (db_ == nullptr) {
            trace.addError("Database context is required for QueryCompilerV3");
            return trace;
        }

        auto addTraceError = [&](std::string message) {
            if (!trace.diagnostic_sql_context().empty()) {
                message.append(" | SQL_CONTEXT: ");
                message.append(trace.diagnostic_sql_context());
            }
            trace.addError(message);
        };

        parser::v3::ParseResult parse_result = parser.parseStatement();
        if (!parse_result.success()) {
            for (const auto& err : parse_result.errors()) {
                addTraceError("Parse error at line " + std::to_string(err.span.start.line) +
                              ", column " + std::to_string(err.span.start.column) + ": " + err.message);
            }
            return trace;
        }
        trace.setDiagnosticSqlContext(
            normalizeSqlForTrace(sql, parse_result.statement(), parser.stringPool()));

        parser::v3::V3Emitter emitter(parser.stringPool());
        sblr::v3::Container container;
        std::string emit_err;
        if (!emitter.emitStatementToContainer(parse_result.statement(), container, emit_err)) {
            addTraceError(emit_err.empty() ? "V3 emit failed" : emit_err);
            return trace;
        }
        container.metadata.module_name = "scratchbird_native";

        auto finalized = detail::finalizeQueryCompilerV3Compilation(db_,
                                                                    sql,
                                                                    parse_result.statement(),
                                                                    parser.stringPool(),
                                                                    current_schema_,
                                                                    optimizations_enabled_,
                                                                    container,
                                                                    nullptr,
                                                                    detail::QueryCompilerV3PlanProfileMode::GENERIC);
        for (const auto& warning : finalized.warnings) {
            trace.addWarning(warning);
        }
        if (!finalized.success) {
            for (const auto& error : finalized.errors) {
                addTraceError(error);
            }
            return trace;
        }

        sblr::v3::Container decoded;
        std::string decode_err;
        if (!sblr::v3::decodeContainer(finalized.bytecode.data(),
                                       finalized.bytecode.size(),
                                       decoded,
                                       decode_err)) {
            addTraceError(decode_err.empty() ? "V3 container decode failed" : decode_err);
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
            addTraceError(instruction_err.message.empty()
                              ? "V3 instruction decode failed for version opcode"
                              : instruction_err.message);
            return trace;
        }
        sblr::v3::Instruction root_inst;
        const size_t root_offset = offset;
        if (!sblr::v3::decodeInstructionWithSchema(decoded.bytecode_stream.data(),
                                                   decoded.bytecode_stream.size(),
                                                   offset,
                                                   root_inst,
                                                   instruction_err)) {
            addTraceError(instruction_err.message.empty()
                              ? "V3 instruction decode failed for root opcode"
                              : instruction_err.message);
            return trace;
        }

        sblr::v3::Buffer root_bytes(decoded.bytecode_stream.begin() + root_offset,
                                    decoded.bytecode_stream.begin() + offset);

        udr::LanguageUdrSblrSqlRenderRequest render_request{};
        render_request.request_id = core::generateUuidV7();
        render_request.profile_id = "native";
        render_request.profile_version = "1.0";
        render_request.native_feature_key = "sblr_to_native_sql_render";
        render_request.principal_id = core::generateUuidV7();
        render_request.role_context_signature = "trace";
        render_request.render_permission_granted = true;
        render_request.root_instruction = root_inst;

        udr::LanguageUdrSblrSqlRenderResponse render_response{};
        core::ErrorContext render_ctx;
        if (udr::renderSblrToNativeSqlEndpoint(
                traceRenderRegistry(),
                render_request,
                render_response,
                &render_ctx) == core::Status::OK &&
            render_response.success &&
            !render_response.sql_text.empty()) {
            trace.setDiagnosticSqlContext(render_response.sql_text);
        }

        const std::string normalized_sql =
            normalizeSqlForTrace(sql, parse_result.statement(), parser.stringPool());
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
        digest.sblr_hash = hashBytesHex(finalized.bytecode);
        digest.root_opcode_symbol = root_symbol;
        trace.setDigest(std::move(digest));
        return trace;
    }

private:
    static auto traceRenderRegistry() -> const udr::LanguageUdrRegistry& {
        static udr::LanguageUdrRegistry registry;
        static std::once_flag init_once;
        std::call_once(init_once, []() {
            udr::LanguageUdrRegistration registration{};
            registration.module_id = core::generateUuidV7();
            registration.module_name = "sb_udr_render_native";
            registration.engine_profile_id = "native";
            registration.engine_profile_version = "1.0";
            registration.translation_mode = "SQL_REWRITE_TO_NATIVE";
            registration.module_semver = "1.0.0";
            registration.artifact_hash = "artifact_native_render";
            registration.signature_status = udr::LanguageUdrSignatureStatus::TRUSTED;
            registration.status = udr::LanguageUdrModuleStatus::ACTIVE;

            core::ErrorContext ctx;
            (void)registry.registerModule(
                registration,
                {"sblr_to_native_sql_render"},
                &ctx);
        });
        return registry;
    }

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

    static std::string toUpperAscii(std::string value) {
        for (char& c : value) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return value;
    }

    static std::string escapeTraceField(std::string_view value) {
        std::string out;
        out.reserve(value.size());
        for (char c : value) {
            if (c == '\\' || c == '|') {
                out.push_back('\\');
            }
            out.push_back(c);
        }
        return out;
    }

    static std::string idToTraceText(const parser::v3::StringPool& pool,
                                     parser::v3::StringPool::StringId id) {
        if (id == parser::v3::StringPool::INVALID_ID) {
            return {};
        }
        return std::string(pool.get(id));
    }

    static std::string udrCanonicalKeyValueTrace(std::string_view family,
                                                 bool validate_only,
                                                 std::string_view key_a_name,
                                                 std::string_view key_a_value,
                                                 std::string_view key_b_name,
                                                 std::string_view key_b_value,
                                                 std::string_view key_c_name,
                                                 std::string_view key_c_value,
                                                 std::string_view key_d_name,
                                                 std::string_view key_d_value) {
        std::string out;
        out.reserve(256);
        out.append(family);
        out.append("|validate=");
        out.push_back(validate_only ? '1' : '0');
        out.push_back('|');
        out.append(key_a_name);
        out.push_back('=');
        out.append(escapeTraceField(key_a_value));
        out.push_back('|');
        out.append(key_b_name);
        out.push_back('=');
        out.append(escapeTraceField(key_b_value));
        out.push_back('|');
        out.append(key_c_name);
        out.push_back('=');
        out.append(escapeTraceField(key_c_value));
        out.push_back('|');
        out.append(key_d_name);
        out.push_back('=');
        out.append(escapeTraceField(key_d_value));
        return out;
    }

    static std::string canonicalAstTraceSql(const parser::v3::Statement* stmt,
                                            const parser::v3::StringPool& pool) {
        if (stmt == nullptr) {
            return {};
        }

        switch (stmt->kind()) {
            case parser::v3::ASTKind::AST_DOC_PATH_FILTER: {
                auto* s = static_cast<const parser::v3::DocPathFilterStmt*>(stmt);
                const char* cmp = "EQ";
                switch (s->compare_op) {
                    case 0: cmp = "EQ"; break;
                    case 1: cmp = "NE"; break;
                    case 2: cmp = "LT"; break;
                    case 3: cmp = "LE"; break;
                    case 4: cmp = "GT"; break;
                    case 5: cmp = "GE"; break;
                    case 6: cmp = "EXISTS"; break;
                    case 7: cmp = "NOT_EXISTS"; break;
                    default: cmp = "UNKNOWN"; break;
                }
                return "DOC PATH FILTER PATH_ID " + std::to_string(s->path_expr) + " OP " + cmp +
                       " VALUE_REF " + std::to_string(s->value_expr);
            }
            case parser::v3::ASTKind::AST_TS_BUCKET_AGG: {
                auto* s = static_cast<const parser::v3::TsBucketAggStmt*>(stmt);
                std::string out = "TS BUCKET AGG TIME_EXPR " + std::to_string(s->time_expr) +
                                  " BUCKET_NS " + std::to_string(s->bucket_size) + " AGG_REFS (";
                for (size_t i = 0; i < s->agg_refs.size(); ++i) {
                    if (i > 0) {
                        out.push_back(',');
                    }
                    out.append(std::to_string(s->agg_refs[i]));
                }
                out.push_back(')');
                return out;
            }
            case parser::v3::ASTKind::AST_SEARCH_QUERY_DSL: {
                auto* s = static_cast<const parser::v3::SearchQueryDslStmt*>(stmt);
                const char* scorer = "BM25";
                switch (s->scorer_id) {
                    case 1: scorer = "BM25"; break;
                    case 2: scorer = "TFIDF"; break;
                    case 3: scorer = "DFR"; break;
                    default: scorer = "UNKNOWN"; break;
                }
                std::string payload = idToTraceText(pool, s->dsl_payload_json);
                return "SEARCH QUERY DSL TARGET_INDEX " + std::to_string(s->target_index) +
                       " PAYLOAD '" + payload + "' SCORER " + scorer;
            }
            case parser::v3::ASTKind::AST_VECTOR_ANN_QUERY: {
                auto* s = static_cast<const parser::v3::VectorAnnQueryStmt*>(stmt);
                const char* metric = "UNKNOWN";
                switch (s->metric) {
                    case 1: metric = "L2"; break;
                    case 2: metric = "COSINE"; break;
                    case 3: metric = "DOT"; break;
                    default: metric = "UNKNOWN"; break;
                }
                return "VECTOR ANN QUERY INDEX " + std::to_string(s->vector_expr) + " METRIC " + metric +
                       " TOPK " + std::to_string(s->k) + " EF_SEARCH " + std::to_string(s->ef_search);
            }
            case parser::v3::ASTKind::AST_HYBRID_BRIDGE: {
                auto* s = static_cast<const parser::v3::HybridBridgeStmt*>(stmt);
                const char* mode = "UNKNOWN";
                switch (s->bridge_mode) {
                    case 1: mode = "HASH_SHUFFLE"; break;
                    case 2: mode = "RANGE_SHUFFLE"; break;
                    case 3: mode = "BROADCAST"; break;
                    default: mode = "UNKNOWN"; break;
                }
                return "HYBRID BRIDGE EXCHANGE SOURCE_TRACK " + std::to_string(s->source_track) +
                       " TARGET_TRACK " + std::to_string(s->target_track) + " MODE " + mode;
            }
            case parser::v3::ASTKind::AST_UDR_COMPILE_DISPATCH: {
                auto* s = static_cast<const parser::v3::UdrCompileDispatchStmt*>(stmt);
                return udrCanonicalKeyValueTrace(
                    "UDR_COMPILE_DISPATCH",
                    s->validate_only,
                    "profile_id",
                    toUpperAscii(idToTraceText(pool, s->profile_id)),
                    "payload_format",
                    toUpperAscii(idToTraceText(pool, s->payload_format)),
                    "payload_bytes",
                    idToTraceText(pool, s->payload_bytes),
                    "session_signature",
                    idToTraceText(pool, s->session_signature));
            }
            case parser::v3::ASTKind::AST_UDR_EMBEDDED_SQL_COMPILE: {
                auto* s = static_cast<const parser::v3::UdrEmbeddedSqlCompileStmt*>(stmt);
                return udrCanonicalKeyValueTrace(
                    "UDR_EMBEDDED_SQL_COMPILE",
                    s->validate_only,
                    "template_id",
                    idToTraceText(pool, s->template_id),
                    "sql_text",
                    idToTraceText(pool, s->sql_text),
                    "profile_id",
                    toUpperAscii(idToTraceText(pool, s->profile_id)),
                    "session_signature",
                    idToTraceText(pool, s->session_signature));
            }
            case parser::v3::ASTKind::AlterSystemStmt: {
                auto* s = static_cast<const parser::v3::AlterSystemStmt*>(stmt);
                if (s->value == nullptr || s->value->kind() != parser::v3::ASTKind::LiteralExpr) {
                    break;
                }
                const auto* lit = static_cast<const parser::v3::LiteralExpr*>(s->value);
                if (lit->literal_type != parser::v3::LiteralType::STRING ||
                    lit->string_value == parser::v3::StringPool::INVALID_ID) {
                    break;
                }
                const std::string key = toUpperAscii(idToTraceText(pool, s->name));
                if (key == "GRAPH.PATH.QUANTIFIED" ||
                    key == "SEARCH.JOIN_FIELD.MAPPING" ||
                    key == "SEARCH.PERCOLATOR.FIELD" ||
                    key == "REDIS.LUA.EVAL" ||
                    key == "REDIS.STREAM.GROUP.CREATE" ||
                    key == "REDIS.STREAM.GROUP.READ" ||
                    key == "REDIS.STREAM.GROUP.CLAIM") {
                    return idToTraceText(pool, lit->string_value);
                }
                break;
            }
            default:
                break;
        }

        return {};
    }

    static std::string normalizeSqlForTrace(const std::string& sql,
                                            const parser::v3::Statement* stmt,
                                            const parser::v3::StringPool& pool) {
        std::string canonical = canonicalAstTraceSql(stmt, pool);
        if (!canonical.empty()) {
            return canonical;
        }

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

        // Native canonical rule: generator mutation normalizes to SET SEQUENCE.
        if (stmt != nullptr && stmt->kind() == parser::v3::ASTKind::SetStmt) {
            const auto* set_stmt = static_cast<const parser::v3::SetStmt*>(stmt);
            if (set_stmt->set_type == parser::v3::SetStmt::SetType::GENERATOR) {
                constexpr const char* kGeneratorPrefix = "SET GENERATOR ";
                constexpr const char* kSequencePrefix = "SET SEQUENCE ";
                if (out.rfind(kGeneratorPrefix, 0) == 0) {
                    out.replace(0, std::char_traits<char>::length(kGeneratorPrefix), kSequencePrefix);
                }
            }
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
