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
 * MySQL Query Compiler
 *
 * Compiles MySQL SQL to SBLR bytecode using the MySQL dialect parser.
 * Pipeline:
 *  - MySQL Lexer/Parser (dialect-aware) -> bytecode (parser-generated)
 *  - Optional catalog context via Database pointer for schema resolution
 */

#include "scratchbird/parser/mysql/mysql_parser.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird {
namespace sblr {

struct MySQLCompilationStats {
    std::chrono::microseconds parser_time{0};
    std::chrono::microseconds total_time{0};
    size_t bytecode_size = 0;
};

class MySQLCompilationResult {
public:
    MySQLCompilationResult() = default;

    bool success() const { return errors_.empty() && !bytecode_.empty(); }
    const std::vector<uint8_t>& bytecode() const { return bytecode_; }
    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }
    const MySQLCompilationStats& stats() const { return stats_; }

    void setBytecode(std::vector<uint8_t> bc) { bytecode_ = std::move(bc); }
    void addError(const std::string& err) { errors_.push_back(err); }
    void addWarning(const std::string& warn) { warnings_.push_back(warn); }
    void setStats(const MySQLCompilationStats& s) { stats_ = s; }

private:
    std::vector<uint8_t> bytecode_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    MySQLCompilationStats stats_;
};

class MySQLQueryCompiler {
public:
    explicit MySQLQueryCompiler(core::Database* db);
    ~MySQLQueryCompiler();

    MySQLQueryCompiler(const MySQLQueryCompiler&) = delete;
    MySQLQueryCompiler& operator=(const MySQLQueryCompiler&) = delete;

    MySQLCompilationResult compile(const std::string& sql);

    // Default schema path for unqualified identifiers
    const std::string& defaultSchema() const { return default_schema_; }
    void setDefaultSchema(const std::string& schema) { default_schema_ = schema; }
    void setCompatibilityMode(parser::mysql::MySQLCompatMode mode) { compat_mode_ = mode; }
    parser::mysql::MySQLCompatMode compatibilityMode() const { return compat_mode_; }

private:
    core::Database* db_;
    core::CatalogManager* catalog_;
    std::string default_schema_;
    bool stats_enabled_ = true;
    parser::mysql::MySQLCompatMode compat_mode_ = parser::mysql::MySQLCompatMode::MYSQL57;

    MySQLCompilationResult compileInternal(const std::string& sql);
};

} // namespace sblr
} // namespace scratchbird
