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
#include <cstdio>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/types.h"
#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/v3_container.h"

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

private:
    core::Database* db_ = nullptr;
    core::ID current_schema_{};
    bool stats_enabled_ = false;
    bool optimizations_enabled_ = true;
};

using CompilationResultV3 = QueryCompilerV3::CompileResult;

}  // namespace scratchbird::sblr
