/**
 * ScratchBird Query Compiler V2 Implementation
 *
 * Phase 9: Integration layer for Parser V2 pipeline
 */

#include "scratchbird/sblr/query_compiler_v2.h"
#include <chrono>
#include <functional>

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
        if (catalog_->getSchema("users.public", public_schema_info, &ctx) == core::Status::OK ||
            catalog_->getSchema("public", public_schema_info, &ctx) == core::Status::OK) {
            current_schema_ = public_schema_info.schema_id;
            default_schema_ = public_schema_info.schema_id;
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
            // QueryResultCache stores results; compilation always runs in this path.
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
    if (!search_path_.empty())
    {
        analyzer.setSearchPath(search_path_);
    }

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
    // Success - set result
    // =========================================================================

    result.setBytecode(bc_result.bytecode());

    // Seed dependencies collected during semantic analysis (domains, catalog-resolved functions/UDRs)
    for (const auto& dep : sem_result.dependencies()) {
        const_cast<std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>>&>(result.dependencies()).push_back(dep);
    }

    // Extract involved tables for cache invalidation
    extractInvolvedTables(sem_result.statement(),
        const_cast<std::unordered_set<core::ID, core::IDHash>&>(result.involvedTables()));

    // Collect broader dependencies (tables, functions, sequences, schemas)
    collectDependencies(sem_result.statement(),
        sem_result.stringPool(),
        const_cast<std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>>&>(result.dependencies()));

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
    } else if (auto* copy = dynamic_cast<ResolvedCopyStmt*>(stmt)) {
        if (copy->has_table && !copy->target_table.table_uuid.bytes[0] == 0) {
            tables.insert(copy->target_table.table_uuid);
        }
        if (copy->query) {
            extractInvolvedTables(copy->query, tables);
        }
    }
}

static std::string toString(parser::v2::StringPool* pool, parser::v2::StringPool::StringId id) {
    if (!pool || id == parser::v2::StringPool::INVALID_ID) return {};
    return std::string(pool->get(id));
}

static bool isZeroUuid(const core::ID& id) {
    for (auto b : id.bytes) {
        if (b != 0) return false;
    }
    return true;
}

void QueryCompilerV2::collectDependencies(ResolvedStatement* stmt,
                             parser::v2::StringPool* pool,
                             std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>>& deps) {
    if (!stmt) return;

    auto add = [&](const core::ID& id, core::CatalogManager::ObjectType type) {
        if (id.bytes[0] == 0 && id.bytes[1] == 0) return;
        deps.emplace_back(id, type);
    };

    auto addByLookup = [&](const std::string& name) -> bool {
        if (name.empty() || !catalog_ || isZeroUuid(default_schema_)) {
            return false;
        }
        core::CatalogManager::ObjectLookup lookup{};
        core::ErrorContext ctx;
        if (catalog_->lookupObject(default_schema_, name, lookup, &ctx) == core::Status::OK) {
            add(lookup.object_id, lookup.type);
            return true;
        }
        return false;
    };

    auto addFunctionByName = [&](const std::string& name) {
        if (name.empty() || !catalog_) return;

        if (addByLookup(name)) return;

        core::CatalogManager::FunctionInfo fi;
        core::ErrorContext ctx;
        if (catalog_->getFunction(name, fi, &ctx) == core::Status::OK) {
            add(fi.function_id, core::CatalogManager::ObjectType::FUNCTION);
            return;
        }
        core::CatalogManager::ProcedureInfo pi;
        if (catalog_->getProcedure(name, pi, &ctx) == core::Status::OK) {
            add(pi.procedure_id, core::CatalogManager::ObjectType::PROCEDURE);
        }
    };

    auto addPackageByName = [&](const std::string& name) {
        if (name.empty() || !catalog_ || isZeroUuid(default_schema_)) return;
        core::CatalogManager::PackageInfo pkg;
        core::ErrorContext ctx;
        if (catalog_->getPackageByName(default_schema_, name, pkg, &ctx) == core::Status::OK) {
            add(pkg.package_id, core::CatalogManager::ObjectType::PACKAGE);
        }
    };

    auto tryAddSequenceFromLiteral = [&](ResolvedExpression* expr) {
        auto* lit = dynamic_cast<ResolvedLiteral*>(expr);
        if (!lit || lit->literal_type != LiteralType::STRING || lit->string_value == parser::v2::StringPool::INVALID_ID || !catalog_) {
            return;
        }
        std::string seq_name = toString(pool, lit->string_value);
        core::ErrorContext ctx;

        auto parseQualifiedName = [](const std::string& name,
                                     std::string& schema_path_out,
                                     std::string& object_name_out) {
            schema_path_out.clear();
            object_name_out.clear();

            if (name.find('/') != std::string::npos) {
                std::vector<std::string> components;
                size_t start = 0;
                while (start < name.size()) {
                    while (start < name.size() && name[start] == '/') {
                        ++start;
                    }
                    if (start >= name.size()) {
                        break;
                    }
                    size_t end = name.find('/', start);
                    if (end == std::string::npos) {
                        end = name.size();
                    }
                    components.emplace_back(name.substr(start, end - start));
                    start = end + 1;
                }
                if (!components.empty()) {
                    object_name_out = components.back();
                    components.pop_back();
                }
                if (!components.empty()) {
                    schema_path_out = components.front();
                    for (size_t i = 1; i < components.size(); ++i) {
                        schema_path_out.push_back('.');
                        schema_path_out.append(components[i]);
                    }
                }
                return;
            }

            size_t dot_pos = name.rfind('.');
            if (dot_pos != std::string::npos) {
                schema_path_out = name.substr(0, dot_pos);
                object_name_out = name.substr(dot_pos + 1);
            } else {
                object_name_out = name;
            }
        };

        std::string schema_path;
        std::string seq_base;
        parseQualifiedName(seq_name, schema_path, seq_base);
        if (seq_base.empty()) {
            return;
        }

        if (!schema_path.empty()) {
            core::CatalogManager::SchemaInfo schema_info;
            if (catalog_->getSchema(schema_path, schema_info, &ctx) == core::Status::OK) {
                core::ID seq_id;
                if (catalog_->getSequenceIdByName(schema_info.schema_id, seq_base, seq_id, &ctx) == core::Status::OK) {
                    add(seq_id, core::CatalogManager::ObjectType::SEQUENCE);
                }
            }
            return;
        }

        if (!isZeroUuid(default_schema_)) {
            core::ID seq_id;
            if (catalog_->getSequenceIdByName(default_schema_, seq_base, seq_id, &ctx) == core::Status::OK) {
                add(seq_id, core::CatalogManager::ObjectType::SEQUENCE);
            }
        }
    };

    std::function<void(ResolvedExpression*)> walkExpr = [&](ResolvedExpression* expr) {
        if (!expr) return;
        if (auto* fn = dynamic_cast<ResolvedFunctionCall*>(expr)) {
            if (!isZeroUuid(fn->function.function_uuid)) {
                core::CatalogManager::ObjectType obj_type = core::CatalogManager::ObjectType::FUNCTION;
                if (fn->function.kind == FunctionKind::UDR) {
                    obj_type = core::CatalogManager::ObjectType::UDR;
                }
                add(fn->function.function_uuid, obj_type);
            } else {
                std::string fname = toString(pool, fn->function.function_name);
                addFunctionByName(fname);
                // Package-qualified names: record package dependency (best-effort)
                auto pos = fname.find('.');
                if (pos != std::string::npos) {
                    addPackageByName(fname.substr(0, pos));
                }
                // Detect sequence functions by name with literal arg
                if (fname == "nextval" || fname == "currval" || fname == "setval") {
                    if (!fn->arguments.empty()) {
                        tryAddSequenceFromLiteral(fn->arguments[0]);
                    }
                }
            }
            for (auto* arg : fn->arguments) walkExpr(arg);
            if (fn->filter) walkExpr(fn->filter);
            if (fn->separator) walkExpr(fn->separator);
        } else if (auto* cast = dynamic_cast<ResolvedCast*>(expr)) {
            walkExpr(cast->expr);
        } else if (auto* case_expr = dynamic_cast<ResolvedCase*>(expr)) {
            if (case_expr->operand) walkExpr(case_expr->operand);
            for (const auto& when : case_expr->when_clauses) {
                walkExpr(when.when_expr);
                walkExpr(when.then_expr);
            }
            if (case_expr->else_expr) walkExpr(case_expr->else_expr);
        } else if (auto* sub = dynamic_cast<ResolvedSubqueryExpr*>(expr)) {
            collectDependencies(sub->subquery, pool, deps);
        }
    };

    if (auto* select = dynamic_cast<ResolvedSelectStmt*>(stmt)) {
        for (const auto& table_ref : select->from_tables) {
            // Use correct object type from semantic analyzer (could be TABLE, VIEW, or MATERIALIZED_VIEW)
            core::CatalogManager::ObjectType obj_type = core::CatalogManager::ObjectType::TABLE;
            if (table_ref->object_type == ResolvedTableRef::ObjectType::VIEW) {
                obj_type = core::CatalogManager::ObjectType::VIEW;
            } else if (table_ref->object_type == ResolvedTableRef::ObjectType::MATERIALIZED_VIEW) {
                obj_type = core::CatalogManager::ObjectType::VIEW;  // Materialized views are still views
            }
            add(table_ref->table_uuid, obj_type);
            add(table_ref->schema_uuid, core::CatalogManager::ObjectType::SCHEMA);
        }
        for (const auto& join : select->joins) {
            if (join->right) {
                // Use correct object type from semantic analyzer
                core::CatalogManager::ObjectType obj_type = core::CatalogManager::ObjectType::TABLE;
                if (join->right->object_type == ResolvedTableRef::ObjectType::VIEW) {
                    obj_type = core::CatalogManager::ObjectType::VIEW;
                } else if (join->right->object_type == ResolvedTableRef::ObjectType::MATERIALIZED_VIEW) {
                    obj_type = core::CatalogManager::ObjectType::VIEW;
                }
                add(join->right->table_uuid, obj_type);
                add(join->right->schema_uuid, core::CatalogManager::ObjectType::SCHEMA);
            }
            if (join->on_condition) walkExpr(join->on_condition);
        }
        if (select->where) walkExpr(select->where);
        for (auto* expr : select->group_by) walkExpr(expr);
        if (select->having) walkExpr(select->having);
        for (auto* ob : select->order_by) walkExpr(ob->expr);
        if (select->limit) walkExpr(select->limit);
        if (select->offset) walkExpr(select->offset);
        for (auto& item : select->select_list) {
            walkExpr(item.expr);
        }
    } else if (auto* insert = dynamic_cast<ResolvedInsertStmt*>(stmt)) {
        add(insert->target_table.table_uuid, core::CatalogManager::ObjectType::TABLE);
        add(insert->target_table.schema_uuid, core::CatalogManager::ObjectType::SCHEMA);
        for (auto& row : insert->values_rows) {
            for (auto* expr : row) walkExpr(expr);
        }
        if (insert->select_source) collectDependencies(insert->select_source, pool, deps);
    } else if (auto* copy = dynamic_cast<ResolvedCopyStmt*>(stmt)) {
        if (copy->has_table) {
            add(copy->target_table.table_uuid, core::CatalogManager::ObjectType::TABLE);
        }
        if (copy->query) {
            collectDependencies(copy->query, pool, deps);
        }
    } else if (auto* update = dynamic_cast<ResolvedUpdateStmt*>(stmt)) {
        add(update->target_table.table_uuid, core::CatalogManager::ObjectType::TABLE);
        add(update->target_table.schema_uuid, core::CatalogManager::ObjectType::SCHEMA);
        for (const auto& jt : update->from_tables) {
            add(jt->table_uuid, core::CatalogManager::ObjectType::TABLE);
            add(jt->schema_uuid, core::CatalogManager::ObjectType::SCHEMA);
        }
        if (update->where) walkExpr(update->where);
        for (const auto& asn : update->assignments) walkExpr(asn.second);
        for (const auto& join : update->joins) {
            if (join->right) {
                add(join->right->table_uuid, core::CatalogManager::ObjectType::TABLE);
                add(join->right->schema_uuid, core::CatalogManager::ObjectType::SCHEMA);
            }
            if (join->on_condition) walkExpr(join->on_condition);
        }
    } else if (auto* del = dynamic_cast<ResolvedDeleteStmt*>(stmt)) {
        add(del->target_table.table_uuid, core::CatalogManager::ObjectType::TABLE);
        add(del->target_table.schema_uuid, core::CatalogManager::ObjectType::SCHEMA);
        if (del->where) walkExpr(del->where);
    }

    // Deduplicate deps vector
    std::unordered_set<core::ID, core::IDHash> seen;
    std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>> deduped;
    deduped.reserve(deps.size());
    for (const auto& d : deps) {
        if (seen.insert(d.first).second) {
            deduped.push_back(d);
        }
    }
    deps.swap(deduped);
}

} // namespace sblr
} // namespace scratchbird
