/**
 * ScratchBird Query Compiler V2 Implementation
 *
 * Phase 9: Integration layer for Parser V2 pipeline
 */

#include "scratchbird/sblr/query_compiler_v2.h"
#include <chrono>

namespace scratchbird {
namespace sblr {

using namespace parser::v2;

QueryCompilerV2::QueryCompilerV2(core::Database* db)
    : db_(db)
    , catalog_(db ? db->catalog_manager() : nullptr)
{
    // Initialize with public schema if available
    if (catalog_) {
        core::CatalogManager::SchemaInfo public_schema_info;
        core::ErrorContext ctx;
        if (catalog_->getSchema("public", public_schema_info, &ctx) == core::Status::OK) {
            current_schema_ = public_schema_info.schema_id;
        }
    }
}

QueryCompilerV2::~QueryCompilerV2() = default;

CompilationResultV2 QueryCompilerV2::compile(const std::string& sql) {
    return compileInternal(sql);
}

CompilationResultV2 QueryCompilerV2::compileWithCache(const std::string& sql, bool use_cache) {
    CompilationResultV2 result;
    auto start_time = std::chrono::steady_clock::now();

    // Check cache first if enabled
    if (use_cache) {
        QueryResultCache& cache = QueryResultCacheManager::getInstance();
        if (cache.isEnabled()) {
            QueryHash hash = QueryResultCache::computeHash(sql);
            // Note: QueryResultCache stores results, not bytecode
            // For bytecode caching, we would need a separate cache
            // For now, we skip bytecode cache and always compile
            // TODO: Add BytecodeCache for compiled query caching
        }
    }

    // Compile
    result = compileInternal(sql);

    // Update stats
    auto end_time = std::chrono::steady_clock::now();
    if (stats_enabled_) {
        CompilationStats stats = result.stats();
        stats.total_time = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        result.setStats(stats);
    }

    return result;
}

CompilationResultV2 QueryCompilerV2::compileInternal(const std::string& sql) {
    CompilationResultV2 result;
    CompilationStats stats;

    auto total_start = std::chrono::steady_clock::now();

    // =========================================================================
    // Phase 1: Lexing and Parsing
    // =========================================================================

    auto parse_start = std::chrono::steady_clock::now();

    Parser parser(sql);
    ParseResult parse_result = parser.parseStatement();

    auto parse_end = std::chrono::steady_clock::now();
    stats.parser_time = std::chrono::duration_cast<std::chrono::microseconds>(
        parse_end - parse_start);

    if (!parse_result.success()) {
        for (const auto& err : parse_result.errors()) {
            result.addError("Parse error at line " + std::to_string(err.span.start.line) +
                          ", column " + std::to_string(err.span.start.column) + ": " + err.message);
        }
        return result;
    }

    // =========================================================================
    // Phase 2: Semantic Analysis
    // =========================================================================

    if (!catalog_) {
        result.addError("Catalog manager not available");
        return result;
    }

    auto semantic_start = std::chrono::steady_clock::now();

    SemanticAnalyzerV2 analyzer(*catalog_, parser.stringPool());
    analyzer.setCurrentSchema(current_schema_);

    SemanticResult sem_result = analyzer.analyze(parse_result.statement());

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
    // Phase 3: Bytecode Generation
    // =========================================================================

    auto bytecode_start = std::chrono::steady_clock::now();

    BytecodeGeneratorV2 generator(parser.stringPool());
    generator.setOptimizationsEnabled(optimizations_enabled_);

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
    // Success - set result
    // =========================================================================

    result.setBytecode(bc_result.bytecode());

    // Extract involved tables for cache invalidation
    extractInvolvedTables(sem_result.statement(),
        const_cast<std::unordered_set<core::ID, core::IDHash>&>(result.involvedTables()));

    // Update stats
    auto total_end = std::chrono::steady_clock::now();
    stats.total_time = std::chrono::duration_cast<std::chrono::microseconds>(
        total_end - total_start);
    stats.bytecode_size = bc_result.bytecode().size();

    result.setStats(stats);

    // Update total stats
    total_stats_.lexer_time += stats.lexer_time;
    total_stats_.parser_time += stats.parser_time;
    total_stats_.semantic_time += stats.semantic_time;
    total_stats_.bytecode_time += stats.bytecode_time;
    total_stats_.total_time += stats.total_time;
    total_stats_.bytecode_size += stats.bytecode_size;

    return result;
}

void QueryCompilerV2::extractInvolvedTables(ResolvedStatement* stmt,
                                            std::unordered_set<core::ID, core::IDHash>& tables) {
    if (!stmt) return;

    // Extract table IDs based on statement type
    if (auto* select = dynamic_cast<ResolvedSelectStmt*>(stmt)) {
        // FROM tables
        for (const auto& table_ref : select->from_tables) {
            if (!table_ref->table_uuid.bytes[0] == 0) {  // Valid UUID
                tables.insert(table_ref->table_uuid);
            }
        }
        // JOIN tables
        for (const auto& join : select->joins) {
            if (join->right && !join->right->table_uuid.bytes[0] == 0) {
                tables.insert(join->right->table_uuid);
            }
        }
    } else if (auto* insert = dynamic_cast<ResolvedInsertStmt*>(stmt)) {
        if (!insert->target_table.table_uuid.bytes[0] == 0) {
            tables.insert(insert->target_table.table_uuid);
        }
    } else if (auto* update = dynamic_cast<ResolvedUpdateStmt*>(stmt)) {
        if (!update->target_table.table_uuid.bytes[0] == 0) {
            tables.insert(update->target_table.table_uuid);
        }
    } else if (auto* del = dynamic_cast<ResolvedDeleteStmt*>(stmt)) {
        if (!del->target_table.table_uuid.bytes[0] == 0) {
            tables.insert(del->target_table.table_uuid);
        }
    }
}

} // namespace sblr
} // namespace scratchbird
