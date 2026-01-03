/**
 * PostgreSQL Query Compiler Implementation
 *
 * Compiles PostgreSQL SQL to SBLR bytecode using the PostgreSQL parser.
 * The PostgreSQL parser generates SBLR bytecode directly.
 *
 * Default schema: remote.emulated.postgresql.localhost
 */

#include "scratchbird/sblr/postgresql_query_compiler.h"
#include <chrono>

namespace scratchbird {
namespace sblr {

// Use explicit namespace prefix
namespace pg = parser::postgresql;

PostgreSQLQueryCompiler::PostgreSQLQueryCompiler(core::Database* db)
    : db_(db)
    , catalog_(db ? db->catalog_manager() : nullptr)
{
    // Initialize with default PostgreSQL schema if available
    if (catalog_) {
        core::CatalogManager::SchemaInfo schema_info;
        core::ErrorContext ctx;

        // Try to find or create the PostgreSQL emulation schema hierarchy:
        // remote.emulated.postgresql.localhost.public
        // For now, use public schema as default
        if (catalog_->getSchema("public", schema_info, &ctx) == core::Status::OK) {
            current_schema_ = schema_info.schema_id;
        }
    }

    // Default search path for PostgreSQL
    search_path_ = {"public"};
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
    // Phase 1: Parsing (PostgreSQL parser generates bytecode directly)
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
    // Success - the PostgreSQL parser generates bytecode directly
    // =========================================================================

    result.setBytecode(parse_result.bytecode());
    stats.bytecode_size = parse_result.bytecode().size();

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
