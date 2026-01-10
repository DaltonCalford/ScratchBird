/**
 * ScratchBird SBLR v2.0 - Bytecode Generator Implementation
 *
 * Generates SBLR bytecode from resolved AST nodes.
 */

#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/core/catalog_manager.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_set>

namespace scratchbird::parser::v2 {

namespace {

uint8_t mapIsolationLevel(IsolationLevel level) {
    // Keep in sync with core::IsolationLevel numeric values.
    switch (level) {
        case IsolationLevel::READ_UNCOMMITTED:
        case IsolationLevel::READ_COMMITTED:
            return 0; // core::IsolationLevel::READ_COMMITTED
        case IsolationLevel::REPEATABLE_READ:
            return 2; // core::IsolationLevel::SNAPSHOT
        case IsolationLevel::SERIALIZABLE:
            return 3; // core::IsolationLevel::SNAPSHOT_TABLE_STABILITY
        default:
            return 0;
    }
}

uint8_t mapWaitMode(TransactionWaitMode mode) {
    return mode == TransactionWaitMode::WAIT ? 1 : 0;
}

using ObjectType = scratchbird::core::CatalogManager::ObjectType;

bool isZeroUuid(const scratchbird::core::ID& id) {
    for (auto byte : id.bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

ObjectType mapTableRefType(ResolvedTableRef::ObjectType type) {
    switch (type) {
        case ResolvedTableRef::ObjectType::TABLE:
            return ObjectType::TABLE;
        case ResolvedTableRef::ObjectType::VIEW:
        case ResolvedTableRef::ObjectType::MATERIALIZED_VIEW:
            return ObjectType::VIEW;
        default:
            return ObjectType::UNKNOWN;
    }
}

void addDependency(std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                   std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen,
                   const scratchbird::core::ID& id,
                   ObjectType type) {
    if (type == ObjectType::UNKNOWN || isZeroUuid(id)) {
        return;
    }
    if (seen.insert(id).second) {
        deps.emplace_back(id, type);
    }
}

void collectDependenciesFromStatement(ResolvedStatement* stmt,
                                      std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                                      std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen);

void collectDependenciesFromExpression(ResolvedExpression* expr,
                                       std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                                       std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen);

void collectDependenciesFromOrderBy(ResolvedOrderByItem* item,
                                    std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                                    std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen) {
    if (!item) {
        return;
    }
    collectDependenciesFromExpression(item->expr, deps, seen);
}

void collectDependenciesFromWindow(ResolvedWindowSpec* window,
                                   std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                                   std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen) {
    if (!window) {
        return;
    }
    for (auto* expr : window->partition_by) {
        collectDependenciesFromExpression(expr, deps, seen);
    }
    for (auto* item : window->order_by) {
        collectDependenciesFromOrderBy(item, deps, seen);
    }
    if (window->has_frame) {
        collectDependenciesFromExpression(window->frame_start.offset, deps, seen);
        collectDependenciesFromExpression(window->frame_end.offset, deps, seen);
    }
}

void collectDependenciesFromExpression(ResolvedExpression* expr,
                                       std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                                       std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen) {
    if (!expr) {
        return;
    }

    if (auto* binary = dynamic_cast<ResolvedBinaryExpr*>(expr)) {
        collectDependenciesFromExpression(binary->left, deps, seen);
        collectDependenciesFromExpression(binary->right, deps, seen);
    } else if (auto* unary = dynamic_cast<ResolvedUnaryExpr*>(expr)) {
        collectDependenciesFromExpression(unary->operand, deps, seen);
    } else if (auto* fn = dynamic_cast<ResolvedFunctionCall*>(expr)) {
        if (!isZeroUuid(fn->function.function_uuid)) {
            ObjectType obj_type = ObjectType::FUNCTION;
            if (fn->function.kind == FunctionKind::UDR) {
                obj_type = ObjectType::UDR;
            }
            addDependency(deps, seen, fn->function.function_uuid, obj_type);
        }
        for (auto* arg : fn->arguments) {
            collectDependenciesFromExpression(arg, deps, seen);
        }
        collectDependenciesFromExpression(fn->filter, deps, seen);
        collectDependenciesFromExpression(fn->separator, deps, seen);
        for (auto* item : fn->internal_order_by) {
            collectDependenciesFromOrderBy(item, deps, seen);
        }
        if (fn->is_window) {
            collectDependenciesFromWindow(fn->window, deps, seen);
        }
    } else if (auto* cast = dynamic_cast<ResolvedCast*>(expr)) {
        collectDependenciesFromExpression(cast->expr, deps, seen);
    } else if (auto* case_expr = dynamic_cast<ResolvedCase*>(expr)) {
        collectDependenciesFromExpression(case_expr->operand, deps, seen);
        for (const auto& when : case_expr->when_clauses) {
            collectDependenciesFromExpression(when.when_expr, deps, seen);
            collectDependenciesFromExpression(when.then_expr, deps, seen);
        }
        collectDependenciesFromExpression(case_expr->else_expr, deps, seen);
    } else if (auto* sub = dynamic_cast<ResolvedSubqueryExpr*>(expr)) {
        collectDependenciesFromStatement(sub->subquery, deps, seen);
    } else if (auto* exists_expr = dynamic_cast<ResolvedExistsExpr*>(expr)) {
        collectDependenciesFromStatement(exists_expr->subquery, deps, seen);
    } else if (auto* in_expr = dynamic_cast<ResolvedInExpr*>(expr)) {
        collectDependenciesFromExpression(in_expr->expr, deps, seen);
        for (auto* value : in_expr->values) {
            collectDependenciesFromExpression(value, deps, seen);
        }
        if (in_expr->has_subquery) {
            collectDependenciesFromStatement(in_expr->subquery, deps, seen);
        }
    } else if (auto* between = dynamic_cast<ResolvedBetweenExpr*>(expr)) {
        collectDependenciesFromExpression(between->expr, deps, seen);
        collectDependenciesFromExpression(between->low, deps, seen);
        collectDependenciesFromExpression(between->high, deps, seen);
    } else if (auto* like_expr = dynamic_cast<ResolvedLikeExpr*>(expr)) {
        collectDependenciesFromExpression(like_expr->expr, deps, seen);
        collectDependenciesFromExpression(like_expr->pattern, deps, seen);
        collectDependenciesFromExpression(like_expr->escape, deps, seen);
    } else if (auto* is_null = dynamic_cast<ResolvedIsNullExpr*>(expr)) {
        collectDependenciesFromExpression(is_null->expr, deps, seen);
    } else if (auto* array_expr = dynamic_cast<ResolvedArrayExpr*>(expr)) {
        for (auto* element : array_expr->elements) {
            collectDependenciesFromExpression(element, deps, seen);
        }
        if (array_expr->has_subquery) {
            collectDependenciesFromStatement(array_expr->subquery, deps, seen);
        }
    }
}

void collectDependenciesFromTableRef(ResolvedTableRef* table_ref,
                                     std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                                     std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen) {
    if (!table_ref) {
        return;
    }
    ObjectType obj_type = mapTableRefType(table_ref->object_type);
    addDependency(deps, seen, table_ref->table_uuid, obj_type);
    addDependency(deps, seen, table_ref->schema_uuid, ObjectType::SCHEMA);
    if (table_ref->subquery) {
        collectDependenciesFromStatement(table_ref->subquery, deps, seen);
    }
}

void collectDependenciesFromStatement(ResolvedStatement* stmt,
                                      std::vector<std::pair<scratchbird::core::ID, ObjectType>>& deps,
                                      std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash>& seen) {
    if (!stmt) {
        return;
    }

    if (auto* select = dynamic_cast<ResolvedSelectStmt*>(stmt)) {
        for (auto* table_ref : select->from_tables) {
            collectDependenciesFromTableRef(table_ref, deps, seen);
        }
        for (auto* join : select->joins) {
            if (join) {
                collectDependenciesFromTableRef(join->left, deps, seen);
                collectDependenciesFromTableRef(join->right, deps, seen);
                collectDependenciesFromExpression(join->on_condition, deps, seen);
            }
        }
        for (auto& item : select->select_list) {
            if (item.item_type == ResolvedSelectItem::ItemType::EXPRESSION) {
                collectDependenciesFromExpression(item.expr, deps, seen);
            }
        }
        collectDependenciesFromExpression(select->where, deps, seen);
        for (auto* expr : select->group_by) {
            collectDependenciesFromExpression(expr, deps, seen);
        }
        collectDependenciesFromExpression(select->having, deps, seen);
        for (auto* item : select->order_by) {
            collectDependenciesFromOrderBy(item, deps, seen);
        }
        collectDependenciesFromExpression(select->limit, deps, seen);
        collectDependenciesFromExpression(select->offset, deps, seen);
        if (select->set_op_right) {
            collectDependenciesFromStatement(select->set_op_right, deps, seen);
        }
    } else if (auto* insert = dynamic_cast<ResolvedInsertStmt*>(stmt)) {
        collectDependenciesFromTableRef(&insert->target_table, deps, seen);
        for (auto& row : insert->values_rows) {
            for (auto* expr : row) {
                collectDependenciesFromExpression(expr, deps, seen);
            }
        }
        if (insert->select_source) {
            collectDependenciesFromStatement(insert->select_source, deps, seen);
        }
    } else if (auto* update = dynamic_cast<ResolvedUpdateStmt*>(stmt)) {
        collectDependenciesFromTableRef(&update->target_table, deps, seen);
        for (const auto& table_ref : update->from_tables) {
            collectDependenciesFromTableRef(table_ref, deps, seen);
        }
        for (const auto& assignment : update->assignments) {
            collectDependenciesFromExpression(assignment.second, deps, seen);
        }
        for (const auto& join : update->joins) {
            if (join) {
                collectDependenciesFromTableRef(join->left, deps, seen);
                collectDependenciesFromTableRef(join->right, deps, seen);
                collectDependenciesFromExpression(join->on_condition, deps, seen);
            }
        }
        collectDependenciesFromExpression(update->where, deps, seen);
    } else if (auto* del = dynamic_cast<ResolvedDeleteStmt*>(stmt)) {
        collectDependenciesFromTableRef(&del->target_table, deps, seen);
        collectDependenciesFromExpression(del->where, deps, seen);
    }
}

std::vector<std::pair<scratchbird::core::ID, ObjectType>> collectDependencies(ResolvedStatement* stmt) {
    std::vector<std::pair<scratchbird::core::ID, ObjectType>> deps;
    std::unordered_set<scratchbird::core::ID, scratchbird::core::IDHash> seen;
    collectDependenciesFromStatement(stmt, deps, seen);
    return deps;
}

} // namespace

// =============================================================================
// BytecodeGeneratorV2 Implementation
// =============================================================================

BytecodeGeneratorV2::BytecodeGeneratorV2(const StringPool& string_pool)
    : string_pool_(string_pool) {}

BytecodeGeneratorV2::~BytecodeGeneratorV2() = default;

BytecodeResultV2 BytecodeGeneratorV2::generate(ResolvedStatement* stmt) {
    BytecodeResultV2 result;
    current_result_ = &result;

    if (!stmt) {
        result.addError("Cannot generate bytecode for null statement");
        return result;
    }

    // Write SBLR version header
    result.writeOpcode(sblr::Opcode::VERSION);
    result.writeByte(sblr::SBLR_VERSION);

    // Generate statement bytecode
    generateStatement(stmt);

    // Write end marker
    result.writeOpcode(sblr::Opcode::END);

    current_result_ = nullptr;
    return result;
}

// =============================================================================
// Statement Generation
// =============================================================================

void BytecodeGeneratorV2::generateStatement(ResolvedStatement* stmt) {
    if (auto* select = dynamic_cast<ResolvedSelectStmt*>(stmt)) {
        generateSelect(select);
    } else if (auto* insert = dynamic_cast<ResolvedInsertStmt*>(stmt)) {
        generateInsert(insert);
    } else if (auto* update = dynamic_cast<ResolvedUpdateStmt*>(stmt)) {
        generateUpdate(update);
    } else if (auto* del = dynamic_cast<ResolvedDeleteStmt*>(stmt)) {
        generateDelete(del);
    } else if (auto* create_table = dynamic_cast<ResolvedCreateTableStmt*>(stmt)) {
        generateCreateTable(create_table);
    } else if (auto* create_index = dynamic_cast<ResolvedCreateIndexStmt*>(stmt)) {
        generateCreateIndex(create_index);
    } else if (auto* create_view = dynamic_cast<ResolvedCreateViewStmt*>(stmt)) {
        generateCreateView(create_view);
    } else if (auto* create_schema = dynamic_cast<ResolvedCreateSchemaStmt*>(stmt)) {
        generateCreateSchema(create_schema);
    } else if (auto* drop_schema = dynamic_cast<ResolvedDropSchemaStmt*>(stmt)) {
        generateDropSchema(drop_schema);
    } else if (auto* alter_schema = dynamic_cast<ResolvedAlterSchemaStmt*>(stmt)) {
        generateAlterSchema(alter_schema);
    } else if (auto* create_database = dynamic_cast<ResolvedCreateDatabaseStmt*>(stmt)) {
        generateCreateDatabase(create_database);
    } else if (auto* create_domain = dynamic_cast<ResolvedCreateDomainStmt*>(stmt)) {
        generateCreateDomain(create_domain);
    } else if (auto* alter_domain = dynamic_cast<ResolvedAlterDomainStmt*>(stmt)) {
        generateAlterDomain(alter_domain);
    } else if (auto* drop_domain = dynamic_cast<ResolvedDropDomainStmt*>(stmt)) {
        generateDropDomain(drop_domain);
    } else if (auto* drop_database = dynamic_cast<ResolvedDropDatabaseStmt*>(stmt)) {
        generateDropDatabase(drop_database);
    } else if (auto* alter_database = dynamic_cast<ResolvedAlterDatabaseStmt*>(stmt)) {
        generateAlterDatabase(alter_database);
    } else if (auto* alter_table = dynamic_cast<ResolvedAlterTableStmt*>(stmt)) {
        generateAlterTable(alter_table);
    } else if (auto* rename_obj = dynamic_cast<ResolvedRenameObjectStmt*>(stmt)) {
        generateRenameObject(rename_obj);
    } else if (auto* move_obj = dynamic_cast<ResolvedMoveObjectStmt*>(stmt)) {
        generateMoveObject(move_obj);
    } else if (auto* drop = dynamic_cast<ResolvedDropStmt*>(stmt)) {
        generateDrop(drop);
    } else if (auto* start_tx = dynamic_cast<ResolvedStartTransactionStmt*>(stmt)) {
        generateStartTransaction(start_tx);
    } else if (auto* prepare_tx = dynamic_cast<ResolvedPrepareTransactionStmt*>(stmt)) {
        generatePrepareTransaction(prepare_tx);
    } else if (auto* commit = dynamic_cast<ResolvedCommitStmt*>(stmt)) {
        generateCommit(commit);
    } else if (auto* rollback = dynamic_cast<ResolvedRollbackStmt*>(stmt)) {
        generateRollback(rollback);
    } else if (auto* savepoint = dynamic_cast<ResolvedSavepointStmt*>(stmt)) {
        generateSavepoint(savepoint);
    } else if (auto* set = dynamic_cast<ResolvedSetStmt*>(stmt)) {
        generateSet(set);
    } else if (auto* show = dynamic_cast<ResolvedShowStmt*>(stmt)) {
        generateShow(show);
    } else if (auto* truncate = dynamic_cast<ResolvedTruncateTableStmt*>(stmt)) {
        generateTruncateTable(truncate);
    } else if (auto* explain = dynamic_cast<ResolvedExplainStmt*>(stmt)) {
        generateExplain(explain);
    } else {
        current_result_->addError("Unknown statement type for bytecode generation");
    }
}

// =============================================================================
// DML Statement Generation
// =============================================================================

void BytecodeGeneratorV2::generateSelect(ResolvedSelectStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::SELECT);

    // Compact stream layout (v2): flags + select list + FROM list (Appendix_A_SBLR_BYTECODE.md).
    uint8_t flags = 0;
    if (stmt->distinct) {
        flags |= 0x01;
    }
    if (stmt->for_update) {
        flags |= 0x02;
    }
    if (stmt->for_share) {
        flags |= 0x04;
    }
    current_result_->writeByte(flags);

    generateSelectList(stmt->select_list);
    generateFromClause(stmt->from_tables, stmt->joins);

    // Generate WHERE clause
    if (stmt->where) {
        generateWhereClause(stmt->where);
    }

    // Generate GROUP BY clause
    if (!stmt->group_by.empty()) {
        generateGroupByClause(stmt->group_by);
    }

    // Generate HAVING clause
    if (stmt->having) {
        generateHavingClause(stmt->having);
    }

    // Generate ORDER BY clause
    if (!stmt->order_by.empty()) {
        generateOrderByClause(stmt->order_by);
    }

    // Generate LIMIT/OFFSET
    if (stmt->limit || stmt->offset) {
        generateLimitOffset(stmt->limit, stmt->offset);
    }

    // Handle set operations (UNION, INTERSECT, EXCEPT)
    if (stmt->set_op != SetOpType::NONE && stmt->set_op_right) {
        switch (stmt->set_op) {
            case SetOpType::UNION:
                if (stmt->set_op_all) {
                    current_result_->writeExtendedOpcode(
                        sblr::ExtendedOpcode::EXT_UNION_ALL);
                } else {
                    current_result_->writeExtendedOpcode(
                        sblr::ExtendedOpcode::EXT_UNION);
                }
                break;
            case SetOpType::INTERSECT:
                if (stmt->set_op_all) {
                    current_result_->writeExtendedOpcode(
                        sblr::ExtendedOpcode::EXT_INTERSECT_ALL);
                } else {
                    current_result_->writeExtendedOpcode(
                        sblr::ExtendedOpcode::EXT_INTERSECT);
                }
                break;
            case SetOpType::EXCEPT:
                if (stmt->set_op_all) {
                    current_result_->writeExtendedOpcode(
                        sblr::ExtendedOpcode::EXT_EXCEPT_ALL);
                } else {
                    current_result_->writeExtendedOpcode(
                        sblr::ExtendedOpcode::EXT_EXCEPT);
                }
                break;
            default:
                break;
        }
        generateSelect(stmt->set_op_right);
    }
}


void BytecodeGeneratorV2::generateInsert(ResolvedInsertStmt* stmt) {
    // Compact stream layout (v2): INSERT + target + column list + source + optional ON CONFLICT/RETURNING.
    current_result_->writeOpcode(sblr::Opcode::INSERT);

    current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
    writeTableRefPayload(stmt->target_table);

    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeListCount(static_cast<uint64_t>(stmt->target_column_indexes.size()));
    for (uint32_t col_idx : stmt->target_column_indexes) {
        if (col_idx < stmt->target_table.columns.size()) {
            current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
            writeStringId(stmt->target_table.columns[col_idx].name);
        }
    }
    current_result_->writeOpcode(sblr::Opcode::END_LIST);

    if (stmt->source == ResolvedInsertStmt::Source::SELECT) {
        if (!stmt->select_source) {
            current_result_->addError("INSERT ... SELECT missing select source");
        } else {
            generateSelect(stmt->select_source);
        }
    } else {
        current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
        uint64_t row_count = 0;
        if (stmt->source == ResolvedInsertStmt::Source::VALUES) {
            row_count = stmt->values_rows.size();
        }
        current_result_->writeListCount(row_count);

        if (stmt->source == ResolvedInsertStmt::Source::VALUES) {
            for (const auto& row : stmt->values_rows) {
                current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
                current_result_->writeListCount(static_cast<uint64_t>(row.size()));
                for (auto* expr : row) {
                    generateExpression(expr);
                }
                current_result_->writeOpcode(sblr::Opcode::END_LIST);
            }
        }

        current_result_->writeOpcode(sblr::Opcode::END_LIST);
    }

    if (stmt->on_conflict) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ON_CONFLICT);

        if (!stmt->on_conflict->conflict_columns.empty()) {
            current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ON_CONFLICT_COLUMN);
            current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
            current_result_->writeListCount(stmt->on_conflict->conflict_columns.size());
            for (uint32_t col_idx : stmt->on_conflict->conflict_columns) {
                if (col_idx < stmt->target_table.columns.size()) {
                    current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
                    writeStringId(stmt->target_table.columns[col_idx].name);
                }
            }
            current_result_->writeOpcode(sblr::Opcode::END_LIST);
        } else if (!isZeroUuid(stmt->on_conflict->constraint_uuid)) {
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_ON_CONFLICT_CONSTRAINT);
            current_result_->writeByte(1);
            current_result_->writeUUID(stmt->on_conflict->constraint_uuid);
        }

        if (stmt->on_conflict->action == ConflictAction::NOTHING) {
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_NOTHING);
        } else if (stmt->on_conflict->action == ConflictAction::UPDATE) {
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE);
            current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
            current_result_->writeListCount(
                static_cast<uint64_t>(stmt->on_conflict->update_assignments.size()));
            for (const auto& [col_idx, expr] : stmt->on_conflict->update_assignments) {
                current_result_->writeOpcode(sblr::Opcode::ASSIGNMENT);
                current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
                if (col_idx < stmt->target_table.columns.size()) {
                    writeStringId(stmt->target_table.columns[col_idx].name);
                } else {
                    current_result_->writeString("?column?");
                }
                generateExpression(expr);
            }
            current_result_->writeOpcode(sblr::Opcode::END_LIST);

            if (stmt->on_conflict->where) {
                current_result_->writeOpcode(sblr::Opcode::WHERE_CLAUSE);
                generateExpression(stmt->on_conflict->where);
            }
        }
    }

    if (!stmt->returning.empty()) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_RETURNING);
        generateSelectList(stmt->returning);
    }
}

void BytecodeGeneratorV2::generateUpdate(ResolvedUpdateStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::UPDATE);

    current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
    writeTableRefPayload(stmt->target_table);

    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeListCount(static_cast<uint64_t>(stmt->assignments.size()));
    for (const auto& [col_idx, expr] : stmt->assignments) {
        current_result_->writeOpcode(sblr::Opcode::ASSIGNMENT);
        current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
        if (col_idx < stmt->target_table.columns.size()) {
            writeStringId(stmt->target_table.columns[col_idx].name);
        } else {
            current_result_->writeString("?column?");
        }
        generateExpression(expr);
    }
    current_result_->writeOpcode(sblr::Opcode::END_LIST);

    if (!stmt->from_tables.empty() || !stmt->joins.empty()) {
        generateFromClause(stmt->from_tables, stmt->joins);
    }

    if (stmt->where) {
        generateWhereClause(stmt->where);
    }

    if (!stmt->returning.empty()) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_RETURNING);
        generateSelectList(stmt->returning);
    }
}

void BytecodeGeneratorV2::generateDelete(ResolvedDeleteStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::DELETE);

    current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
    writeTableRefPayload(stmt->target_table);

    if (!stmt->using_tables.empty() || !stmt->using_joins.empty()) {
        generateFromClause(stmt->using_tables, stmt->using_joins);
    }

    if (stmt->where) {
        generateWhereClause(stmt->where);
    }

    if (!stmt->returning.empty()) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_RETURNING);
        generateSelectList(stmt->returning);
    }
}

// =============================================================================
// DDL Statement Generation
// =============================================================================

void BytecodeGeneratorV2::generateCreateTable(ResolvedCreateTableStmt* stmt) {
    // Compact stream layout (v2): CREATE_TABLE, TABLE_REF, column list, tablespace, constraints.

    current_result_->writeOpcode(sblr::Opcode::CREATE_TABLE);

    // Write table name with TABLE_REF opcode
    current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
    writeTableRefPayload(core::ID{}, stmt->table_name, false, StringPool::INVALID_ID);

    // Write BEGIN_LIST opcode for columns
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeListCount(static_cast<uint64_t>(stmt->columns.size()));

    // Write each column definition in the current format
    for (const auto& col : stmt->columns) {
        current_result_->writeOpcode(sblr::Opcode::COLUMN_DEF);

        // Write COLUMN_REF with qualifier (empty) and name
        current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
        current_result_->writeString("");  // Empty qualifier
        writeStringId(col.name);

        // Write data type opcode
        generateDataType(col.type);

        // Write NOT NULL constraint if column is not nullable
        if (!col.is_nullable) {
            current_result_->writeOpcode(sblr::Opcode::NOT_NULL);
        }

        // Write DEFAULT value as serialized bytecode
        if (col.default_value) {
            current_result_->writeOpcode(sblr::Opcode::DEFAULT_VALUE);

            // Generate expression bytecode to a temporary buffer
            BytecodeResultV2 temp_result;
            BytecodeResultV2* saved_result = current_result_;
            current_result_ = &temp_result;
            generateExpression(col.default_value);
            current_result_ = saved_result;

            // Write bytecode length and data
            const auto& bytecode = temp_result.bytecode();
            current_result_->writeInt32(static_cast<uint32_t>(bytecode.size()));
            for (uint8_t b : bytecode) {
                current_result_->writeByte(b);
            }
        }

        // Write CHECK constraint as serialized bytecode
        if (col.check_expr) {
            current_result_->writeOpcode(sblr::Opcode::CHECK_CONSTRAINT);

            // Generate expression bytecode to a temporary buffer
            BytecodeResultV2 temp_result;
            BytecodeResultV2* saved_result = current_result_;
            current_result_ = &temp_result;
            generateExpression(col.check_expr);
            current_result_ = saved_result;

            // Write bytecode length and data
            const auto& bytecode = temp_result.bytecode();
            current_result_->writeInt32(static_cast<uint32_t>(bytecode.size()));
            for (uint8_t b : bytecode) {
                current_result_->writeByte(b);
            }

            // Write constraint name (empty for column-level)
            current_result_->writeString("");
        }

        // Note: PRIMARY KEY and UNIQUE at column level are handled via table constraints
        // Foreign key constraints are handled separately as well
    }

    // Write END_LIST opcode
    current_result_->writeOpcode(sblr::Opcode::END_LIST);

    // Write tablespace name (empty string if none)
    current_result_->writeString("");  // No tablespace support in v2 yet

    // Write table-level FK constraints
    for (const auto& constraint : stmt->constraints) {
        if (constraint.constraint_type == ResolvedTableConstraint::Type::FOREIGN_KEY) {
            current_result_->writeOpcode(sblr::Opcode::TABLE_FK);

            // Write child column count and names
            current_result_->writeByte(static_cast<uint8_t>(constraint.column_indexes.size()));
            for (uint32_t idx : constraint.column_indexes) {
                if (idx < stmt->columns.size()) {
                    writeStringId(stmt->columns[idx].name);
                }
            }

            // Write parent table name (we need to look this up - use UUID for now)
            // For simplicity, write a placeholder - FK support needs more work
            current_result_->writeString("");  // Parent table name placeholder

            // Write parent column count and names
            current_result_->writeByte(static_cast<uint8_t>(constraint.fk_column_indexes.size()));
            for (uint32_t idx : constraint.fk_column_indexes) {
                current_result_->writeInt32(idx);  // Column index in parent
            }

            // Write ON DELETE action
            current_result_->writeString("NO ACTION");  // Default

            // Write ON UPDATE action
            current_result_->writeString("NO ACTION");  // Default

            // Write constraint name
            if (constraint.name != StringPool::INVALID_ID) {
                writeStringId(constraint.name);
            } else {
                current_result_->writeString("");
            }

            // Write deferrable flags
            current_result_->writeByte(0);  // Not deferrable
        }
    }
}

void BytecodeGeneratorV2::generateCreateIndex(ResolvedCreateIndexStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::CREATE_INDEX);

    writeStringId(stmt->index_name);

    if (stmt->table_path != StringPool::INVALID_ID) {
        writeStringId(stmt->table_path);
    } else {
        current_result_->writeString("");
    }

    current_result_->writeByte(stmt->unique ? 1 : 0);

    current_result_->writeInt32(static_cast<uint32_t>(stmt->column_names.size()));
    for (const auto& col_name : stmt->column_names) {
        writeStringId(col_name);
    }

    if (stmt->tablespace_name != StringPool::INVALID_ID) {
        writeStringId(stmt->tablespace_name);
    } else {
        current_result_->writeString("");
    }

    uint8_t index_type = 0xFF;
    std::string_view method = getString(stmt->index_method);
    if (!method.empty()) {
        std::string lower(method);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lower == "btree") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
        } else if (lower == "hash") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
        } else if (lower == "gin") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIN);
        } else if (lower == "gist") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIST);
        } else if (lower == "brin") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BRIN);
        } else if (lower == "spgist") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::SPGIST);
        } else if (lower == "bitmap") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BITMAP);
        }
    }

    current_result_->writeByte(index_type);

    bool has_expressions = false;
    bool has_predicate = (stmt->where_clause != nullptr);
    current_result_->writeByte(has_expressions ? 1 : 0);
    current_result_->writeByte(has_predicate ? 1 : 0);

    if (has_predicate)
    {
        BytecodeResultV2 temp_result;
        BytecodeResultV2* saved_result = current_result_;
        current_result_ = &temp_result;
        generateExpression(stmt->where_clause);
        current_result_ = saved_result;

        const auto& bytecode = temp_result.bytecode();
        current_result_->writeInt32(static_cast<uint32_t>(bytecode.size()));
        for (uint8_t b : bytecode)
        {
            current_result_->writeByte(b);
        }
        current_result_->writeString("");
    }
}

void BytecodeGeneratorV2::generateCreateView(ResolvedCreateViewStmt* stmt) {
    if (stmt->materialized) {
        current_result_->writeOpcode(sblr::Opcode::REFRESH_MATERIALIZED_VIEW);
    } else {
        current_result_->writeOpcode(sblr::Opcode::CREATE_VIEW);
    }

    std::vector<std::pair<scratchbird::core::ID, ObjectType>> deps;
    if (stmt->query) {
        deps = collectDependencies(stmt->query);
    }

    // Write flags
    uint8_t flags = 0;
    if (stmt->or_replace) flags |= 0x01;
    if (stmt->materialized) flags |= 0x02;
    if (stmt->check_option) flags |= 0x04;
    if (!deps.empty()) flags |= 0x10;
    current_result_->writeByte(flags);

    // Write schema UUID
    current_result_->writeUUID(stmt->schema.schema_uuid);

    // Write view name
    writeStringId(stmt->view_name);

    // Write column names if specified
    current_result_->writeListCount(static_cast<uint64_t>(stmt->column_names.size()));
    for (auto col_name : stmt->column_names) {
        writeStringId(col_name);
    }

    // Write query
    if (stmt->query) {
        generateSelect(stmt->query);
    }

    if (!deps.empty()) {
        current_result_->writeListCount(static_cast<uint64_t>(deps.size()));
        for (const auto& dep : deps) {
            current_result_->writeUUID(dep.first);
            current_result_->writeByte(static_cast<uint8_t>(dep.second));
        }
    }
}

void BytecodeGeneratorV2::generateCreateSchema(ResolvedCreateSchemaStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
    current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_SCHEMA));

    uint8_t flags = stmt->if_not_exists ? 0x01 : 0x00;
    current_result_->writeByte(flags);

    current_result_->writeString(schemaPathToString(stmt->schema_path, string_pool_));

    if (stmt->owner != StringPool::INVALID_ID) {
        writeStringId(stmt->owner);
    } else {
        current_result_->writeString("");
    }
}

void BytecodeGeneratorV2::generateDropSchema(ResolvedDropSchemaStmt* stmt) {
    uint8_t flags = 0;
    if (stmt->if_exists) flags |= 0x01;
    if (stmt->cascade) flags |= 0x02;
    if (stmt->restrict) flags |= 0x04;

    for (const auto& path : stmt->schema_paths) {
        current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
        current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_SCHEMA));
        current_result_->writeByte(flags);
        current_result_->writeString(schemaPathToString(path, string_pool_));
    }
}

void BytecodeGeneratorV2::generateCreateDatabase(ResolvedCreateDatabaseStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
    current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DATABASE));

    uint8_t flags = stmt->if_not_exists ? 0x01 : 0x00;
    current_result_->writeByte(flags);
    current_result_->writeString(schemaPathToString(stmt->database_path, string_pool_));
    current_result_->writeString(stmt->source_spec);
    current_result_->writeInt32(static_cast<uint32_t>(stmt->options.size()));
    for (const auto& opt : stmt->options) {
        current_result_->writeString(opt.key);
        current_result_->writeString(opt.value);
    }
    current_result_->writeInt32(static_cast<uint32_t>(stmt->aliases.size()));
    for (const auto& alias : stmt->aliases) {
        current_result_->writeString(alias);
    }
}

void BytecodeGeneratorV2::generateCreateDomain(ResolvedCreateDomainStmt* stmt) {
    // Spec: docs/specifications/SBLR_DOMAIN_PAYLOADS.md
    current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
    current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));

    uint8_t flags = 0;
    if (stmt->if_not_exists) {
        flags |= 0x01;
    }
    if (stmt->has_integrity) {
        flags |= 0x02;
    }
    if (stmt->has_security) {
        flags |= 0x04;
    }
    if (stmt->has_validation) {
        flags |= 0x08;
    }
    if (stmt->has_quality) {
        flags |= 0x10;
    }
    current_result_->writeByte(flags);

    current_result_->writeByte(static_cast<uint8_t>(stmt->domain_kind));
    current_result_->writeString(schemaPathToString(stmt->domain_path, string_pool_));

    switch (stmt->domain_kind) {
        case DomainKind::BASIC:
            generateDataType(stmt->base_type);
            break;
        case DomainKind::RECORD:
            current_result_->writeListCount(static_cast<uint64_t>(stmt->record_fields.size()));
            for (const auto& field : stmt->record_fields) {
                writeStringId(field.name);
                writeTypeRef(field.type);
                current_result_->writeByte(field.nullable ? 1 : 0);
                current_result_->writeString(field.default_value);
            }
            break;
        case DomainKind::ENUM:
            current_result_->writeListCount(static_cast<uint64_t>(stmt->enum_values.size()));
            for (const auto& value : stmt->enum_values) {
                writeStringId(value.label);
                current_result_->writeInt32(static_cast<uint32_t>(value.position));
            }
            current_result_->writeByte(stmt->enum_wrap ? 1 : 0);
            break;
        case DomainKind::SET:
            writeTypeRef(stmt->set_element_type);
            break;
        case DomainKind::VARIANT:
            current_result_->writeListCount(static_cast<uint64_t>(stmt->variant_allowed_types.size()));
            for (const auto& type_ref : stmt->variant_allowed_types) {
                writeTypeRef(type_ref);
            }
            break;
    }

    current_result_->writeByte(stmt->nullable ? 1 : 0);
    current_result_->writeString(stmt->default_value);
    current_result_->writeString(stmt->has_collation ? stmt->collation_name : std::string());

    current_result_->writeListCount(static_cast<uint64_t>(stmt->constraints.size()));
    for (const auto& constraint : stmt->constraints) {
        current_result_->writeByte(static_cast<uint8_t>(constraint.type));
        if (constraint.name != StringPool::INVALID_ID) {
            writeStringId(constraint.name);
        } else {
            current_result_->writeString("");
        }
        current_result_->writeString(constraint.expression);
    }

    current_result_->writeByte(stmt->has_inherits ? 1 : 0);
    if (stmt->has_inherits) {
        current_result_->writeUUID(stmt->parent_domain_id);
    }

    current_result_->writeString(stmt->has_dialect ? stmt->dialect_tag : std::string());
    current_result_->writeString(stmt->has_compat ? stmt->compat_name : std::string());

    if (stmt->has_integrity) {
        current_result_->writeByte(stmt->integrity.uniqueness ? 1 : 0);
        current_result_->writeByte(stmt->integrity.normalization_enabled ? 1 : 0);
        current_result_->writeString(stmt->integrity.normalization_function);
    }

    if (stmt->has_security) {
        uint8_t sec_flags = 0;
        if (stmt->security.has_masking) sec_flags |= 0x01;
        if (stmt->security.has_mask_pattern) sec_flags |= 0x02;
        if (stmt->security.has_encryption) sec_flags |= 0x04;
        if (stmt->security.has_audit_access) sec_flags |= 0x08;
        if (stmt->security.has_required_privilege) sec_flags |= 0x10;
        current_result_->writeByte(sec_flags);
        if (stmt->security.has_masking) {
            current_result_->writeString(stmt->security.masking);
        }
        if (stmt->security.has_mask_pattern) {
            current_result_->writeString(stmt->security.mask_pattern);
        }
        if (stmt->security.has_encryption) {
            current_result_->writeString(stmt->security.encryption);
        }
        if (stmt->security.has_audit_access) {
            current_result_->writeByte(stmt->security.audit_access ? 1 : 0);
        }
        if (stmt->security.has_required_privilege) {
            current_result_->writeString(stmt->security.required_privilege);
        }
    }

    if (stmt->has_validation) {
        uint8_t val_flags = 0;
        if (stmt->validation.has_function) val_flags |= 0x01;
        if (stmt->validation.has_error_message) val_flags |= 0x02;
        current_result_->writeByte(val_flags);
        if (stmt->validation.has_function) {
            current_result_->writeString(stmt->validation.function);
        }
        if (stmt->validation.has_error_message) {
            current_result_->writeString(stmt->validation.error_message);
        }
    }

    if (stmt->has_quality) {
        uint8_t qual_flags = 0;
        if (stmt->quality.has_parse_function) qual_flags |= 0x01;
        if (stmt->quality.has_standardize_function) qual_flags |= 0x02;
        if (stmt->quality.has_enrich_function) qual_flags |= 0x04;
        current_result_->writeByte(qual_flags);
        if (stmt->quality.has_parse_function) {
            current_result_->writeString(stmt->quality.parse_function);
        }
        if (stmt->quality.has_standardize_function) {
            current_result_->writeString(stmt->quality.standardize_function);
        }
        if (stmt->quality.has_enrich_function) {
            current_result_->writeString(stmt->quality.enrich_function);
        }
    }
}

void BytecodeGeneratorV2::generateDropDatabase(ResolvedDropDatabaseStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
    current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_DATABASE));

    uint8_t flags = 0;
    if (stmt->if_exists) flags |= 0x01;
    if (stmt->force) flags |= 0x02;
    current_result_->writeByte(flags);
    current_result_->writeString(schemaPathToString(stmt->database_path, string_pool_));
}

void BytecodeGeneratorV2::generateAlterDomain(ResolvedAlterDomainStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
    current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DOMAIN));
    current_result_->writeByte(static_cast<uint8_t>(stmt->action));
    current_result_->writeString(schemaPathToString(stmt->domain_path, string_pool_));

    switch (stmt->action) {
        case AlterDomainAction::SET_DEFAULT:
        case AlterDomainAction::ADD_CHECK:
        case AlterDomainAction::SET_COMPAT:
            current_result_->writeString(stmt->value);
            break;
        case AlterDomainAction::DROP_CONSTRAINT:
            if (stmt->constraint_name != StringPool::INVALID_ID) {
                writeStringId(stmt->constraint_name);
            } else {
                current_result_->writeString("");
            }
            break;
        case AlterDomainAction::RENAME:
            if (stmt->new_name != StringPool::INVALID_ID) {
                writeStringId(stmt->new_name);
            } else {
                current_result_->writeString("");
            }
            break;
        case AlterDomainAction::DROP_DEFAULT:
        case AlterDomainAction::DROP_COMPAT:
            break;
    }
}

void BytecodeGeneratorV2::generateDropDomain(ResolvedDropDomainStmt* stmt) {
    uint8_t flags = 0;
    if (stmt->if_exists) flags |= 0x01;
    if (stmt->restrict) flags |= 0x02;

    for (const auto& path : stmt->domains) {
        current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
        current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_DOMAIN));
        current_result_->writeByte(flags);
        current_result_->writeString(schemaPathToString(path, string_pool_));
    }
}

void BytecodeGeneratorV2::generateAlterSchema(ResolvedAlterSchemaStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
    current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_SCHEMA));
    current_result_->writeByte(static_cast<uint8_t>(stmt->action));
    current_result_->writeString(schemaPathToString(stmt->schema_path, string_pool_));

    switch (stmt->action) {
        case AlterSchemaAction::RENAME:
            if (stmt->new_name != StringPool::INVALID_ID) {
                writeStringId(stmt->new_name);
            } else {
                current_result_->writeString("");
            }
            break;
        case AlterSchemaAction::SET_OWNER:
            if (stmt->owner != StringPool::INVALID_ID) {
                writeStringId(stmt->owner);
            } else {
                current_result_->writeString("");
            }
            break;
        case AlterSchemaAction::SET_PATH:
            current_result_->writeString(schemaPathToString(stmt->new_path, string_pool_));
            break;
        default:
            current_result_->writeString("");
            break;
    }
}

void BytecodeGeneratorV2::generateAlterDatabase(ResolvedAlterDatabaseStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
    current_result_->writeInt16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DATABASE));
    current_result_->writeByte(static_cast<uint8_t>(stmt->action));
    current_result_->writeString(schemaPathToString(stmt->database_path, string_pool_));

    switch (stmt->action) {
        case AlterDatabaseAction::RENAME:
            if (stmt->new_name != StringPool::INVALID_ID) {
                writeStringId(stmt->new_name);
            } else {
                current_result_->writeString("");
            }
            break;
        case AlterDatabaseAction::SET_OWNER:
            if (stmt->owner != StringPool::INVALID_ID) {
                writeStringId(stmt->owner);
            } else {
                current_result_->writeString("");
            }
            break;
        case AlterDatabaseAction::ADD_ALIAS:
        case AlterDatabaseAction::DROP_ALIAS:
            current_result_->writeString(stmt->alias);
            break;
        default:
            current_result_->writeString("");
            break;
    }
}

void BytecodeGeneratorV2::generateAlterTable(ResolvedAlterTableStmt* stmt) {
    if (!stmt) {
        return;
    }

    auto writeQualifiedTableName = [&]() -> bool {
        if (stmt->qualified_table_name == StringPool::INVALID_ID) {
            current_result_->addError("ALTER TABLE requires a qualified table name");
            return false;
        }
        writeStringId(stmt->qualified_table_name);
        return true;
    };

    auto resolveTypeModifiers = [&](const ResolvedType& type, uint32_t& precision,
                                    uint32_t& scale) {
        precision = 0;
        scale = 0;

        if (type.length) {
            precision = static_cast<uint32_t>(*type.length);
        } else if (type.precision) {
            precision = static_cast<uint32_t>(*type.precision);
        }

        if (type.scale) {
            scale = static_cast<uint32_t>(*type.scale);
        }

        if ((type.data_type == DataType::VARCHAR || type.data_type == DataType::CHAR) &&
            precision == 0) {
            precision = 255;
        }
        if (type.data_type == DataType::DECIMAL && precision == 0) {
            precision = 18;
        }
    };

    switch (stmt->action) {
        case AlterTableAction::ADD_COLUMN: {
            if (!stmt->has_column_def || stmt->column_def.name == StringPool::INVALID_ID) {
                current_result_->addError("ALTER TABLE ADD COLUMN missing column definition");
                return;
            }
            if (stmt->column_def.type.data_type == DataType::UNKNOWN) {
                current_result_->addError("ALTER TABLE ADD COLUMN has unsupported data type");
                return;
            }

            current_result_->writeOpcode(sblr::Opcode::ALTER_TABLE);
            if (!writeQualifiedTableName()) return;

            current_result_->writeByte(0);  // ADD_COLUMN
            writeStringId(stmt->column_def.name);
            current_result_->writeInt16(static_cast<uint16_t>(stmt->column_def.type.data_type));

            uint32_t precision = 0;
            uint32_t scale = 0;
            resolveTypeModifiers(stmt->column_def.type, precision, scale);
            current_result_->writeInt32(precision);
            current_result_->writeInt32(scale);
            current_result_->writeByte(stmt->column_def.is_nullable ? 1 : 0);
            break;
        }
        case AlterTableAction::DROP_COLUMN: {
            if (stmt->column_name == StringPool::INVALID_ID) {
                current_result_->addError("ALTER TABLE DROP COLUMN requires a column name");
                return;
            }

            current_result_->writeOpcode(sblr::Opcode::ALTER_TABLE);
            if (!writeQualifiedTableName()) return;

            current_result_->writeByte(1);  // DROP_COLUMN
            writeStringId(stmt->column_name);
            current_result_->writeByte(0);  // if_exists (column-level not supported)
            current_result_->writeByte(stmt->cascade ? 1 : 0);
            break;
        }
        case AlterTableAction::ALTER_COLUMN: {
            if (!stmt->has_column_def || stmt->column_name == StringPool::INVALID_ID) {
                current_result_->addError("ALTER TABLE ALTER COLUMN missing type definition");
                return;
            }
            if (stmt->column_def.type.data_type == DataType::UNKNOWN) {
                current_result_->addError("ALTER TABLE ALTER COLUMN has unsupported data type");
                return;
            }

            current_result_->writeOpcode(sblr::Opcode::ALTER_TABLE);
            if (!writeQualifiedTableName()) return;

            current_result_->writeByte(2);  // ALTER_COLUMN_TYPE
            writeStringId(stmt->column_name);
            current_result_->writeInt16(static_cast<uint16_t>(stmt->column_def.type.data_type));

            uint32_t precision = 0;
            uint32_t scale = 0;
            resolveTypeModifiers(stmt->column_def.type, precision, scale);
            current_result_->writeInt32(precision);
            current_result_->writeInt32(scale);
            break;
        }
        case AlterTableAction::SET_TABLESPACE: {
            if (stmt->tablespace_name == StringPool::INVALID_ID) {
                current_result_->addError("ALTER TABLE SET TABLESPACE requires a tablespace name");
                return;
            }
            current_result_->writeOpcode(sblr::Opcode::ALTER_TABLE_SET_TABLESPACE);
            if (!writeQualifiedTableName()) return;
            writeStringId(stmt->tablespace_name);
            current_result_->writeByte(stmt->tablespace_online ? 1 : 0);
            break;
        }
        case AlterTableAction::ENABLE_RLS:
        case AlterTableAction::DISABLE_RLS: {
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_ALTER_TABLE_RLS);
            if (!writeQualifiedTableName()) return;
            current_result_->writeByte(stmt->rls_action);
            break;
        }
        default:
            current_result_->addError("ALTER TABLE action not supported in bytecode generator");
            break;
    }
}

void BytecodeGeneratorV2::generateRenameObject(ResolvedRenameObjectStmt* stmt) {
    current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_RENAME_OBJECT);

    uint8_t flags = 0;
    if (stmt->has_uuid) flags |= 0x01;
    if (stmt->if_exists) flags |= 0x02;
    current_result_->writeByte(flags);
    current_result_->writeByte(static_cast<uint8_t>(stmt->object_type));

    if (stmt->has_uuid) {
        current_result_->writeUUID(stmt->object_uuid);
    }

    writeObjectPath(stmt->object_path);
    writeString16(getString(stmt->new_name));
}

void BytecodeGeneratorV2::generateMoveObject(ResolvedMoveObjectStmt* stmt) {
    current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_MOVE_OBJECT);

    uint8_t flags = 0;
    if (stmt->has_uuid) flags |= 0x01;
    if (stmt->if_exists) flags |= 0x02;
    current_result_->writeByte(flags);
    current_result_->writeByte(static_cast<uint8_t>(stmt->object_type));

    if (stmt->has_uuid) {
        current_result_->writeUUID(stmt->object_uuid);
    }

    writeObjectPath(stmt->object_path);
    writeObjectPath(stmt->target_schema);
    if (stmt->has_new_name) {
        writeString16(getString(stmt->new_name));
    } else {
        writeString16(std::string_view());
    }
}

void BytecodeGeneratorV2::generateDrop(ResolvedDropStmt* stmt) {
    switch (stmt->object_type) {
        case ResolvedDropStmt::ObjectType::TABLE:
            current_result_->writeOpcode(sblr::Opcode::DROP_TABLE);
            break;
        case ResolvedDropStmt::ObjectType::VIEW:
            current_result_->writeOpcode(sblr::Opcode::DROP_VIEW);
            break;
        case ResolvedDropStmt::ObjectType::INDEX:
            current_result_->writeOpcode(sblr::Opcode::DROP_INDEX);
            break;
        case ResolvedDropStmt::ObjectType::SEQUENCE:
            current_result_->writeOpcode(sblr::Opcode::DROP_SEQUENCE);
            break;
    }

    // Write flags
    uint8_t flags = 0;
    if (stmt->if_exists) flags |= 0x01;
    if (stmt->cascade) flags |= 0x02;
    current_result_->writeByte(flags);

    // Write object count and UUIDs
    current_result_->writeListCount(static_cast<uint64_t>(stmt->object_uuids.size()));
    for (const auto& uuid : stmt->object_uuids) {
        current_result_->writeUUID(uuid);
    }
}

// =============================================================================
// Transaction/Session Statement Generation
// =============================================================================

void BytecodeGeneratorV2::generateStartTransaction(ResolvedStartTransactionStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::START_TRANSACTION);

    uint16_t flags = 0;
    if (stmt->has_isolation_level) flags |= sblr::TransactionFlags::HAS_ISOLATION;
    if (stmt->has_access_mode) flags |= sblr::TransactionFlags::HAS_ACCESS_MODE;
    if (stmt->has_read_committed_mode) flags |= sblr::TransactionFlags::HAS_READ_COMMITTED_MODE;
    if (stmt->has_deferrable) flags |= sblr::TransactionFlags::HAS_DEFERRABLE;
    if (stmt->has_wait_mode) flags |= sblr::TransactionFlags::HAS_WAIT_MODE;
    if (stmt->has_lock_timeout) flags |= sblr::TransactionFlags::HAS_LOCK_TIMEOUT;
    if (!stmt->table_reservations.empty()) flags |= sblr::TransactionFlags::HAS_RESERVATIONS;
    if (stmt->has_autocommit) flags |= sblr::TransactionFlags::HAS_AUTOCOMMIT;
    if (stmt->has_conflict_error_code) flags |= sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE;

    current_result_->writeInt16(flags);
    current_result_->writeByte(static_cast<uint8_t>(stmt->conflict_action));

    if (flags & sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE) {
        current_result_->writeInt32(static_cast<uint32_t>(stmt->conflict_error_code));
    }
    if (flags & sblr::TransactionFlags::HAS_AUTOCOMMIT) {
        current_result_->writeByte(static_cast<uint8_t>(stmt->autocommit_mode));
    }
    if (flags & sblr::TransactionFlags::HAS_ISOLATION) {
        current_result_->writeByte(mapIsolationLevel(stmt->isolation_level));
    }
    if (flags & sblr::TransactionFlags::HAS_READ_COMMITTED_MODE) {
        current_result_->writeByte(static_cast<uint8_t>(stmt->read_committed_mode));
    }
    if (flags & sblr::TransactionFlags::HAS_ACCESS_MODE) {
        current_result_->writeByte(static_cast<uint8_t>(stmt->access_mode));
    }
    if (flags & sblr::TransactionFlags::HAS_DEFERRABLE) {
        current_result_->writeByte(stmt->deferrable ? 1 : 0);
    }
    if (flags & sblr::TransactionFlags::HAS_WAIT_MODE) {
        current_result_->writeByte(mapWaitMode(stmt->wait_mode));
    }
    if (flags & sblr::TransactionFlags::HAS_LOCK_TIMEOUT) {
        current_result_->writeInt32(stmt->lock_timeout_seconds);
    }
    if (flags & sblr::TransactionFlags::HAS_RESERVATIONS) {
        // V2 reservation list encoding matches executor expectations (BEGIN_LIST/TABLE_REF/END_LIST).
        current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
        current_result_->writeListCount(static_cast<uint64_t>(stmt->table_reservations.size()));
        for (const auto& reservation : stmt->table_reservations) {
            current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
            writeTableRefPayload(core::ID{}, reservation.table_name, false, StringPool::INVALID_ID);
            current_result_->writeByte(static_cast<uint8_t>(reservation.lock_mode));
            current_result_->writeByte(reservation.for_write ? 1 : 0);
        }
        current_result_->writeOpcode(sblr::Opcode::END_LIST);
    }
}

void BytecodeGeneratorV2::generatePrepareTransaction(ResolvedPrepareTransactionStmt* stmt) {
    current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_PREPARE_TRANSACTION);
    writeStringId(stmt->gid);
}

void BytecodeGeneratorV2::generateCommit(ResolvedCommitStmt* stmt) {
    if (stmt->is_prepared && stmt->prepared_gid != StringPool::INVALID_ID) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_COMMIT_PREPARED);
        writeStringId(stmt->prepared_gid);
        return;
    }

    current_result_->writeOpcode(sblr::Opcode::COMMIT);
    uint8_t flags = 0;
    if (stmt->and_chain) {
        flags |= sblr::CommitRollbackFlags::AND_CHAIN;
    }
    if (stmt->and_no_chain) {
        flags |= sblr::CommitRollbackFlags::AND_NO_CHAIN;
    }
    if (stmt->retaining) {
        flags |= sblr::CommitRollbackFlags::RETAINING;
    }
    if (flags == 0) {
        flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;
    }
    current_result_->writeByte(flags);
}

void BytecodeGeneratorV2::generateRollback(ResolvedRollbackStmt* stmt) {
    if (stmt->is_prepared && stmt->prepared_gid != StringPool::INVALID_ID) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ROLLBACK_PREPARED);
        writeStringId(stmt->prepared_gid);
        return;
    }

    if (stmt->to_savepoint && stmt->savepoint_name != StringPool::INVALID_ID) {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_ROLLBACK_TO_SAVEPOINT);
        writeStringId(stmt->savepoint_name);
    } else {
        current_result_->writeOpcode(sblr::Opcode::ROLLBACK);
        uint8_t flags = 0;
        if (stmt->and_chain) {
            flags |= sblr::CommitRollbackFlags::AND_CHAIN;
        }
        if (stmt->and_no_chain) {
            flags |= sblr::CommitRollbackFlags::AND_NO_CHAIN;
        }
        if (stmt->retaining) {
            flags |= sblr::CommitRollbackFlags::RETAINING;
        }
        if (flags == 0) {
            flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;
        }
        current_result_->writeByte(flags);
    }
}

void BytecodeGeneratorV2::generateSavepoint(ResolvedSavepointStmt* stmt) {
    current_result_->writeExtendedOpcode(
        sblr::ExtendedOpcode::EXT_SAVEPOINT);
    writeStringId(stmt->name);
}

void BytecodeGeneratorV2::generateSet(ResolvedSetStmt* stmt) {
    // Handle different SET types
    switch (stmt->set_type) {
        case SetStmt::SetType::TRANSACTION:
            current_result_->writeOpcode(sblr::Opcode::SET_TRANSACTION);
            {
                uint16_t flags = 0;
                if (stmt->has_isolation_level) flags |= sblr::TransactionFlags::HAS_ISOLATION;
                if (stmt->has_access_mode) flags |= sblr::TransactionFlags::HAS_ACCESS_MODE;
                if (stmt->has_read_committed_mode) flags |= sblr::TransactionFlags::HAS_READ_COMMITTED_MODE;
                if (stmt->has_deferrable) flags |= sblr::TransactionFlags::HAS_DEFERRABLE;
                if (stmt->has_wait_mode) flags |= sblr::TransactionFlags::HAS_WAIT_MODE;
                if (stmt->has_lock_timeout) flags |= sblr::TransactionFlags::HAS_LOCK_TIMEOUT;
                if (!stmt->table_reservations.empty())
                    flags |= sblr::TransactionFlags::HAS_RESERVATIONS;
                if (stmt->has_autocommit) flags |= sblr::TransactionFlags::HAS_AUTOCOMMIT;
                if (stmt->has_conflict_error_code)
                    flags |= sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE;

                current_result_->writeInt16(flags);
                current_result_->writeByte(static_cast<uint8_t>(stmt->conflict_action));

                if (flags & sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE) {
                    current_result_->writeInt32(static_cast<uint32_t>(stmt->conflict_error_code));
                }
                if (flags & sblr::TransactionFlags::HAS_AUTOCOMMIT) {
                    current_result_->writeByte(static_cast<uint8_t>(stmt->autocommit_mode));
                }
                if (flags & sblr::TransactionFlags::HAS_ISOLATION) {
                    current_result_->writeByte(mapIsolationLevel(stmt->isolation_level));
                }
                if (flags & sblr::TransactionFlags::HAS_READ_COMMITTED_MODE) {
                    current_result_->writeByte(static_cast<uint8_t>(stmt->read_committed_mode));
                }
                if (flags & sblr::TransactionFlags::HAS_ACCESS_MODE) {
                    current_result_->writeByte(static_cast<uint8_t>(stmt->access_mode));
                }
                if (flags & sblr::TransactionFlags::HAS_DEFERRABLE) {
                    current_result_->writeByte(stmt->deferrable ? 1 : 0);
                }
                if (flags & sblr::TransactionFlags::HAS_WAIT_MODE) {
                    current_result_->writeByte(mapWaitMode(stmt->wait_mode));
                }
                if (flags & sblr::TransactionFlags::HAS_LOCK_TIMEOUT) {
                    current_result_->writeInt32(stmt->lock_timeout_seconds);
                }
                if (flags & sblr::TransactionFlags::HAS_RESERVATIONS) {
                    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
                    current_result_->writeListCount(
                        static_cast<uint64_t>(stmt->table_reservations.size()));
                    for (const auto& reservation : stmt->table_reservations) {
                        current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
                        writeTableRefPayload(core::ID{}, reservation.table_name, false,
                                             StringPool::INVALID_ID);
                        current_result_->writeByte(static_cast<uint8_t>(reservation.lock_mode));
                        current_result_->writeByte(reservation.for_write ? 1 : 0);
                    }
                    current_result_->writeOpcode(sblr::Opcode::END_LIST);
                }
            }
            break;
        case SetStmt::SetType::AUTOCOMMIT:
            current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_SET_AUTOCOMMIT);
            current_result_->writeByte(
                stmt->autocommit_mode == AutocommitMode::ON ? 1 : 0);
            current_result_->writeByte(static_cast<uint8_t>(stmt->conflict_action));
            if (stmt->conflict_action == TransactionConflictAction::ERROR) {
                int32_t code = stmt->has_conflict_error_code ? stmt->conflict_error_code : 0;
                current_result_->writeInt32(static_cast<uint32_t>(code));
            }
            break;

        case SetStmt::SetType::SQL_DIALECT:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SET_SQL_DIALECT);
            current_result_->writeByte(stmt->sql_dialect);
            break;

        case SetStmt::SetType::NAMES:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SET_NAMES);
            writeStringId(stmt->variable_name);
            break;

        case SetStmt::SetType::LOCAL_TIMEOUT:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SET_LOCAL_TIMEOUT);
            current_result_->writeInt32(stmt->local_timeout_seconds);
            break;

        case SetStmt::SetType::ROLE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SET_ROLE);
            if (stmt->variable_name != StringPool::INVALID_ID) {
                writeStringId(stmt->variable_name);
            } else {
                current_result_->writeInt32(0);  // RESET ROLE
            }
            break;

        case SetStmt::SetType::SESSION_AUTHORIZATION:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SET_SESSION_AUTH);
            if (stmt->variable_name != StringPool::INVALID_ID) {
                writeStringId(stmt->variable_name);
            } else {
                current_result_->writeInt32(0);  // RESET
            }
            break;

        default:
            // Generic SET variable = value
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SET_VARIABLE);
            // Write variable name
            writeStringId(stmt->variable_name);
            // Write value (or 0 for DEFAULT/RESET)
            if (stmt->is_default || stmt->value == nullptr) {
                current_result_->writeByte(0);  // DEFAULT/RESET marker
            } else {
                current_result_->writeByte(1);  // Has value
                generateExpression(stmt->value);
            }
            break;
    }
}

void BytecodeGeneratorV2::generateShow(ResolvedShowStmt* stmt) {
    switch (stmt->show_type) {
        // Session variable commands
        case ShowStmt::ShowType::VARIABLE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_VARIABLE);
            writeStringId(stmt->variable_name);
            break;

        case ShowStmt::ShowType::ALL:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_ALL);
            break;

        case ShowStmt::ShowType::TRANSACTION_ISOLATION_LEVEL:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_TRANSACTION_LEVEL);
            break;

        // Basic catalog queries (MySQL/PostgreSQL style)
        case ShowStmt::ShowType::TABLES:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_TABLES);
            writeStringId(stmt->from_name);      // Optional FROM database
            writeStringId(stmt->like_pattern);   // Optional LIKE pattern
            break;

        case ShowStmt::ShowType::DATABASES:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_DATABASES);
            writeStringId(stmt->like_pattern);   // Optional LIKE pattern
            break;

        case ShowStmt::ShowType::COLUMNS:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_COLUMNS);
            writeStringId(stmt->from_name);      // Required FROM table
            writeStringId(stmt->like_pattern);   // Optional LIKE pattern
            break;

        case ShowStmt::ShowType::INDEXES:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_INDEXES);
            writeStringId(stmt->from_name);      // Required FROM table
            break;

        case ShowStmt::ShowType::CREATE_TABLE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_CREATE_TABLE);
            writeStringId(stmt->variable_name);  // Table name
            break;

        // Firebird ISQL style (detailed object info)
        case ShowStmt::ShowType::TABLE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_TABLE);
            writeStringId(stmt->variable_name);  // Table name
            break;

        case ShowStmt::ShowType::INDEX:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_INDEX);
            writeStringId(stmt->variable_name);  // Index name
            break;

        case ShowStmt::ShowType::TRIGGER:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_TRIGGER);
            writeStringId(stmt->variable_name);  // Trigger name
            break;

        case ShowStmt::ShowType::VIEW:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_VIEW);
            writeStringId(stmt->variable_name);  // View name
            break;

        case ShowStmt::ShowType::PROCEDURE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_PROCEDURE);
            writeStringId(stmt->variable_name);  // Procedure name
            break;

        case ShowStmt::ShowType::FUNCTION:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_FUNCTION);
            writeStringId(stmt->variable_name);  // Function name
            break;

        case ShowStmt::ShowType::DOMAIN:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_DOMAIN);
            writeStringId(stmt->variable_name);  // Domain name
            break;

        case ShowStmt::ShowType::GENERATOR:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_GENERATOR);
            writeStringId(stmt->variable_name);  // Generator/sequence name
            break;

        case ShowStmt::ShowType::SCHEMA:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_SCHEMA);
            writeStringId(stmt->variable_name);  // Optional schema name
            break;

        case ShowStmt::ShowType::ROLE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_ROLE);
            writeStringId(stmt->variable_name);  // Role name
            break;

        case ShowStmt::ShowType::GRANTS:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_GRANTS);
            writeStringId(stmt->variable_name);  // Optional FOR object_name
            break;

        case ShowStmt::ShowType::CHECKS:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_CHECKS);
            writeStringId(stmt->variable_name);  // Table name
            break;

        case ShowStmt::ShowType::COLLATIONS:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_COLLATIONS);
            writeStringId(stmt->like_pattern);   // Optional LIKE pattern
            break;

        case ShowStmt::ShowType::COMMENTS:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_COMMENTS);
            writeStringId(stmt->variable_name);  // Optional object name
            break;

        case ShowStmt::ShowType::DEPENDENCIES:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_DEPENDENCIES);
            writeStringId(stmt->variable_name);  // Optional object name
            break;

        case ShowStmt::ShowType::PACKAGE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_PACKAGE);
            writeStringId(stmt->variable_name);  // Package name
            break;

        case ShowStmt::ShowType::SQL_DIALECT:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_SQL_DIALECT);
            break;

        case ShowStmt::ShowType::VERSION:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_VERSION);
            break;

        case ShowStmt::ShowType::DATABASE:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_DATABASE);
            break;

        case ShowStmt::ShowType::SYSTEM:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_SYSTEM);
            break;

        default:
            current_result_->addError("Unsupported SHOW type");
            break;
    }
}

void BytecodeGeneratorV2::generateTruncateTable(ResolvedTruncateTableStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::TRUNCATE_TABLE);

    // Write flags
    uint8_t flags = 0;
    if (stmt->cascade) flags |= 0x01;
    if (stmt->restart_identity) flags |= 0x02;
    if (stmt->async_mode) flags |= 0x04;  // ASYNC is default
    current_result_->writeByte(flags);

    // Write table count and UUIDs
    current_result_->writeListCount(static_cast<uint64_t>(stmt->table_uuids.size()));
    for (const auto& uuid : stmt->table_uuids) {
        current_result_->writeUUID(uuid);
    }
}

void BytecodeGeneratorV2::generateExplain(ResolvedExplainStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::EXPLAIN_PLAN);

    // Write flags
    uint8_t flags = 0;
    if (stmt->analyze) flags |= 0x01;
    if (stmt->verbose) flags |= 0x02;
    if (stmt->costs) flags |= 0x04;
    if (stmt->buffers) flags |= 0x08;
    if (stmt->timing) flags |= 0x10;
    if (stmt->format_json) flags |= 0x20;
    if (stmt->format_xml) flags |= 0x40;
    if (stmt->format_yaml) flags |= 0x80;
    current_result_->writeByte(flags);

    // Generate the query to explain
    if (stmt->query) {
        generateStatement(stmt->query);
    }
}

// =============================================================================
// Expression Generation
// =============================================================================

void BytecodeGeneratorV2::generateExpression(ResolvedExpression* expr) {
    if (!expr) {
        current_result_->writeOpcode(sblr::Opcode::LITERAL_NULL);
        return;
    }

    // Try constant folding if enabled
    if (optimizations_enabled_) {
        if (auto* folded = tryConstantFold(expr)) {
            generateLiteral(folded);
            return;
        }
    }

    if (auto* lit = dynamic_cast<ResolvedLiteral*>(expr)) {
        generateLiteral(lit);
    } else if (auto* col = dynamic_cast<ResolvedColumnRefExpr*>(expr)) {
        generateColumnRef(col);
    } else if (auto* binary = dynamic_cast<ResolvedBinaryExpr*>(expr)) {
        generateBinaryExpr(binary);
    } else if (auto* unary = dynamic_cast<ResolvedUnaryExpr*>(expr)) {
        generateUnaryExpr(unary);
    } else if (auto* func = dynamic_cast<ResolvedFunctionCall*>(expr)) {
        generateFunctionCall(func);
    } else if (auto* cast = dynamic_cast<ResolvedCast*>(expr)) {
        generateCast(cast);
    } else if (auto* case_expr = dynamic_cast<ResolvedCase*>(expr)) {
        generateCase(case_expr);
    } else if (auto* subq = dynamic_cast<ResolvedSubqueryExpr*>(expr)) {
        generateSubquery(subq);
    } else if (auto* exists = dynamic_cast<ResolvedExistsExpr*>(expr)) {
        generateExists(exists);
    } else if (auto* in = dynamic_cast<ResolvedInExpr*>(expr)) {
        generateIn(in);
    } else if (auto* between = dynamic_cast<ResolvedBetweenExpr*>(expr)) {
        generateBetween(between);
    } else if (auto* like = dynamic_cast<ResolvedLikeExpr*>(expr)) {
        generateLike(like);
    } else if (auto* is_null = dynamic_cast<ResolvedIsNullExpr*>(expr)) {
        generateIsNull(is_null);
    } else if (auto* arr = dynamic_cast<ResolvedArrayExpr*>(expr)) {
        generateArray(arr);
    } else if (auto* extract = dynamic_cast<ResolvedExtractExpr*>(expr)) {
        generateExtract(extract);
    } else if (auto* alter = dynamic_cast<ResolvedAlterElementExpr*>(expr)) {
        generateAlterElement(alter);
    } else {
        current_result_->addError("Unknown expression type for bytecode generation");
    }
}

void BytecodeGeneratorV2::generateLiteral(ResolvedLiteral* expr) {
    if (expr->is_null) {
        current_result_->writeOpcode(sblr::Opcode::LITERAL_NULL);
        return;
    }

    switch (expr->literal_type) {
        case LiteralType::INTEGER:
            if (expr->int_value >= INT32_MIN && expr->int_value <= INT32_MAX) {
                current_result_->writeOpcode(sblr::Opcode::LITERAL_INT32);
                current_result_->writeInt32(static_cast<uint32_t>(expr->int_value));
            } else {
                current_result_->writeOpcode(sblr::Opcode::LITERAL_INT64);
                current_result_->writeInt64(static_cast<uint64_t>(expr->int_value));
            }
            break;

        case LiteralType::FLOAT:
            current_result_->writeOpcode(sblr::Opcode::LITERAL_DOUBLE);
            current_result_->writeDouble(expr->float_value);
            break;

        case LiteralType::BOOLEAN:
            // Boolean is stored as int32: 1 for true, 0 for false
            current_result_->writeOpcode(sblr::Opcode::LITERAL_INT32);
            current_result_->writeInt32(expr->bool_value ? 1 : 0);
            break;

        case LiteralType::STRING:
            current_result_->writeOpcode(sblr::Opcode::LITERAL_STRING);
            writeStringId(expr->string_value);
            break;

        case LiteralType::BLOB:
            current_result_->writeOpcode(sblr::Opcode::LITERAL_STRING);
            writeStringId(expr->string_value);
            break;

        default:
            current_result_->writeOpcode(sblr::Opcode::LITERAL_NULL);
            break;
    }
}

void BytecodeGeneratorV2::generateColumnRef(ResolvedColumnRefExpr* expr) {
    // Column reference by name (resolved in executor against current row context).
    current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
    writeStringId(expr->column.column_name);
}

void BytecodeGeneratorV2::generateBinaryExpr(ResolvedBinaryExpr* expr) {
    // Generate operands first (postfix notation)
    generateExpression(expr->left);
    generateExpression(expr->right);

    if (expr->op == BinaryOp::CONCAT) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_FUNC_CONCAT);
        current_result_->writeByte(2);
        return;
    }

    if (expr->op == BinaryOp::REGEX_MATCH) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_REGEX_MATCH);
        return;
    }
    if (expr->op == BinaryOp::REGEX_MATCH_CI) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_REGEX_MATCH_CI);
        return;
    }
    if (expr->op == BinaryOp::REGEX_NOT_MATCH) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_REGEX_NOT_MATCH);
        return;
    }
    if (expr->op == BinaryOp::REGEX_NOT_MATCH_CI) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_REGEX_NOT_MATCH_CI);
        return;
    }

    // Generate operator
    auto opcode = binaryOpToOpcode(expr->op);
    current_result_->writeOpcode(opcode);

    // JSON operators are encoded like functions (opcode + arg_count)
    if (expr->op == BinaryOp::JSON_EXTRACT || expr->op == BinaryOp::JSON_EXTRACT_TEXT ||
        expr->op == BinaryOp::JSON_HASH_EXTRACT || expr->op == BinaryOp::JSON_HASH_EXTRACT_TEXT) {
        current_result_->writeByte(2);
    }
}

void BytecodeGeneratorV2::generateUnaryExpr(ResolvedUnaryExpr* expr) {
    generateExpression(expr->operand);

    switch (expr->op) {
        case UnaryOp::NEGATE:
            // Implement as 0 - operand
            current_result_->writeOpcode(sblr::Opcode::LITERAL_INT32);
            current_result_->writeInt32(0);
            // Swap operands (operand is on stack, 0 just pushed)
            // Actually for postfix we need: 0, operand, SUBTRACT
            // But we generated operand first, so we need to handle differently
            // For simplicity, just use SUBTRACT (semantics: second - first = -operand)
            current_result_->writeOpcode(sblr::Opcode::EXPR_SUBTRACT);
            break;

        case UnaryOp::NOT:
            // Implement as comparison to 0
            current_result_->writeOpcode(sblr::Opcode::LITERAL_INT32);
            current_result_->writeInt32(0);
            current_result_->writeOpcode(sblr::Opcode::EXPR_EQ);
            break;

        case UnaryOp::BIT_NOT:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_BIT_NOT);
            break;

        default:
            current_result_->addError("Unsupported unary operator");
            break;
    }
}

void BytecodeGeneratorV2::generateFunctionCall(ResolvedFunctionCall* expr) {
    // Generate arguments first
    for (auto* arg : expr->arguments) {
        generateExpression(arg);
    }

    auto arg_count = expr->arguments.size();
    auto write_arg_count = [&]() {
        if (arg_count > std::numeric_limits<uint8_t>::max()) {
            current_result_->addError("Function argument count exceeds byte limit");
            current_result_->writeByte(0);
            return;
        }
        current_result_->writeByte(static_cast<uint8_t>(arg_count));
    };

    // Spec: docs/specifications/INTERNAL_FUNCTIONS.md
    std::string func_name(getString(expr->function.function_name));
    std::transform(func_name.begin(), func_name.end(), func_name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    // Check for aggregate functions
    if (expr->function.is_aggregate) {
        if (func_name == "COUNT") {
            current_result_->writeOpcode(sblr::Opcode::AGG_COUNT);
        } else if (func_name == "SUM") {
            current_result_->writeOpcode(sblr::Opcode::AGG_SUM);
        } else if (func_name == "AVG") {
            current_result_->writeOpcode(sblr::Opcode::AGG_AVG);
        } else if (func_name == "MIN") {
            current_result_->writeOpcode(sblr::Opcode::AGG_MIN);
        } else if (func_name == "MAX") {
            current_result_->writeOpcode(sblr::Opcode::AGG_MAX);
        } else if (func_name == "STDDEV" || func_name == "STDDEV_SAMP") {
            current_result_->writeOpcode(sblr::Opcode::AGG_STDDEV_SAMP);
        } else if (func_name == "STDDEV_POP") {
            current_result_->writeOpcode(sblr::Opcode::AGG_STDDEV_POP);
        } else if (func_name == "VARIANCE" || func_name == "VAR_SAMP") {
            current_result_->writeOpcode(sblr::Opcode::AGG_VAR_SAMP);
        } else if (func_name == "VAR_POP") {
            current_result_->writeOpcode(sblr::Opcode::AGG_VAR_POP);
        } else if (func_name == "CORR") {
            current_result_->writeOpcode(sblr::Opcode::AGG_CORR);
        } else if (func_name == "COVAR_POP") {
            current_result_->writeOpcode(sblr::Opcode::AGG_COVAR_POP);
        } else if (func_name == "ARRAY_AGG") {
            current_result_->writeOpcode(sblr::Opcode::ARRAY_AGG);
        } else {
            // Generic function call
            writeStringId(expr->function.function_name);
            current_result_->writeListCount(static_cast<uint64_t>(expr->arguments.size()));
            return;
        }
        write_arg_count();
        return;
    }

    if (!expr->function.is_builtin) {
        if (expr->function.kind == FunctionKind::PROCEDURE) {
            current_result_->addError("Procedures cannot be used in expressions");
            return;
        }
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_EXPR_FUNCTION_CALL);
        current_result_->writeByte(static_cast<uint8_t>(expr->function.kind));
        current_result_->writeUUID(expr->function.function_uuid);
        write_arg_count();
        return;
    }

    // Built-in functions
    // String functions
    if (func_name == "LENGTH" || func_name == "LEN") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_LENGTH);
        write_arg_count();
    } else if (func_name == "UPPER") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_UPPER);
        write_arg_count();
    } else if (func_name == "LOWER") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_LOWER);
        write_arg_count();
    } else if (func_name == "TRIM") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_TRIM);
        write_arg_count();
    } else if (func_name == "LTRIM") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_FUNC_LTRIM);
        write_arg_count();
    } else if (func_name == "RTRIM") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_FUNC_RTRIM);
        write_arg_count();
    } else if (func_name == "SUBSTRING" || func_name == "SUBSTR") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_SUBSTRING);
        write_arg_count();
    } else if (func_name == "CHAR_LENGTH") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_CHAR_LENGTH);
        write_arg_count();
    } else if (func_name == "OCTET_LENGTH") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_OCTET_LENGTH);
        write_arg_count();
    } else if (func_name == "CONVERT") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_CONVERT);
        write_arg_count();
    } else if (func_name == "COLLATE") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_COLLATE);
        write_arg_count();
    } else if (func_name == "CONCAT") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_FUNC_CONCAT);
        write_arg_count();
    } else if (func_name == "CONCAT_WS") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_FUNC_CONCAT_WS);
        write_arg_count();
    } else if (func_name == "FORMAT_TYPE") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_FORMAT_TYPE);
        write_arg_count();
    } else if (func_name == "OBJ_DESCRIPTION") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_OBJ_DESCRIPTION);
        write_arg_count();
    } else if (func_name == "COL_DESCRIPTION") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_COL_DESCRIPTION);
        write_arg_count();
    } else if (func_name == "SHOBJ_DESCRIPTION") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_SHOBJ_DESCRIPTION);
        write_arg_count();
    }
    // Date/time functions
    else if (func_name == "NOW" || func_name == "CURRENT_TIMESTAMP") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_NOW);
        write_arg_count();
    } else if (func_name == "CURRENT_DATE") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_CURRENT_DATE);
        write_arg_count();
    } else if (func_name == "CURRENT_TIME") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_CURRENT_TIME);
        write_arg_count();
    } else if (func_name == "DATE_ADD") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_DATE_ADD);
        write_arg_count();
    } else if (func_name == "DATE_SUB") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_DATE_SUB);
        write_arg_count();
    } else if (func_name == "DATE_DIFF" || func_name == "DATEDIFF") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_DATE_DIFF);
        write_arg_count();
    }
    // Spatial functions
    else if (func_name == "ST_POINT") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ST_POINT);
        write_arg_count();
    } else if (func_name == "ST_MAKELINE") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ST_MAKELINE);
        write_arg_count();
    } else if (func_name == "ST_MAKEPOLYGON") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ST_MAKEPOLYGON);
        write_arg_count();
    } else if (func_name == "ST_ASTEXT") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ST_ASTEXT);
        write_arg_count();
    } else if (func_name == "ST_ASBINARY") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ST_ASBINARY);
        write_arg_count();
    } else if (func_name == "ST_GEOMETRYTYPE") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ST_GEOMETRYTYPE);
        write_arg_count();
    } else if (func_name == "ST_ISVALID") {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ST_ISVALID);
        write_arg_count();
    }
    // Math functions
    else if (func_name == "ABS") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ABS);
        write_arg_count();
    } else if (func_name == "SIGN") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_SIGN);
        write_arg_count();
    } else if (func_name == "ROUND") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ROUND);
        write_arg_count();
    } else if (func_name == "FLOOR") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_FLOOR);
        write_arg_count();
    } else if (func_name == "CEIL" || func_name == "CEILING") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_CEIL);
        write_arg_count();
    } else if (func_name == "TRUNC") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_TRUNC);
        write_arg_count();
    } else if (func_name == "MOD") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_MOD);
        write_arg_count();
    } else if (func_name == "SQRT") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_SQRT);
        write_arg_count();
    } else if (func_name == "CBRT") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_CBRT);
        write_arg_count();
    } else if (func_name == "POWER" || func_name == "POW") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_POWER);
        write_arg_count();
    } else if (func_name == "EXP") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_EXP);
        write_arg_count();
    } else if (func_name == "LN") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_LN);
        write_arg_count();
    } else if (func_name == "LOG") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_LOG);
        write_arg_count();
    } else if (func_name == "LOG10") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_LOG10);
        write_arg_count();
    } else if (func_name == "LOG2") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_LOG2);
        write_arg_count();
    } else if (func_name == "SIN") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_SIN);
        write_arg_count();
    } else if (func_name == "COS") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_COS);
        write_arg_count();
    } else if (func_name == "TAN") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_TAN);
        write_arg_count();
    } else if (func_name == "ASIN") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ASIN);
        write_arg_count();
    } else if (func_name == "ACOS") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ACOS);
        write_arg_count();
    } else if (func_name == "ATAN") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ATAN);
        write_arg_count();
    } else if (func_name == "ATAN2") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ATAN2);
        write_arg_count();
    } else if (func_name == "DEGREES") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_DEGREES);
        write_arg_count();
    } else if (func_name == "RADIANS") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_RADIANS);
        write_arg_count();
    } else if (func_name == "PI") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_PI);
        write_arg_count();
    } else if (func_name == "SINH") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_SINH);
        write_arg_count();
    } else if (func_name == "COSH") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_COSH);
        write_arg_count();
    } else if (func_name == "TANH") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_TANH);
        write_arg_count();
    } else if (func_name == "ASINH") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ASINH);
        write_arg_count();
    } else if (func_name == "ACOSH") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ACOSH);
        write_arg_count();
    } else if (func_name == "ATANH") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ATANH);
        write_arg_count();
    } else if (func_name == "COT") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_COT);
        write_arg_count();
    }
    // JSON functions
    else if (func_name == "JSON_EXTRACT") {
        current_result_->writeOpcode(sblr::Opcode::JSON_EXTRACT);
        write_arg_count();
    } else if (func_name == "JSON_OBJECT") {
        current_result_->writeOpcode(sblr::Opcode::JSON_OBJECT);
        write_arg_count();
    } else if (func_name == "JSON_ARRAY") {
        current_result_->writeOpcode(sblr::Opcode::JSON_ARRAY);
        write_arg_count();
    } else if (func_name == "JSON_SET") {
        current_result_->writeOpcode(sblr::Opcode::JSON_SET);
        write_arg_count();
    } else if (func_name == "JSON_INSERT") {
        current_result_->writeOpcode(sblr::Opcode::JSON_INSERT);
        write_arg_count();
    } else if (func_name == "JSON_REMOVE") {
        current_result_->writeOpcode(sblr::Opcode::JSON_REMOVE);
        write_arg_count();
    } else if (func_name == "JSONB_EXTRACT_PATH") {
        current_result_->writeOpcode(sblr::Opcode::JSONB_EXTRACT_PATH);
        write_arg_count();
    } else if (func_name == "JSONB_BUILD_OBJECT") {
        current_result_->writeOpcode(sblr::Opcode::JSONB_BUILD_OBJECT);
        write_arg_count();
    } else if (func_name == "JSONB_BUILD_ARRAY") {
        current_result_->writeOpcode(sblr::Opcode::JSONB_BUILD_ARRAY);
        write_arg_count();
    } else if (func_name == "JSONB_SET") {
        current_result_->writeOpcode(sblr::Opcode::JSONB_SET);
        write_arg_count();
    }
    // Null handling
    else if (func_name == "COALESCE") {
        current_result_->writeOpcode(sblr::Opcode::COALESCE);
        write_arg_count();
    } else if (func_name == "NULLIF") {
        current_result_->writeOpcode(sblr::Opcode::NULLIF);
    }
    // Generic function call
    else {
        current_result_->addError("Unsupported built-in function: " + func_name);
    }
}

void BytecodeGeneratorV2::generateCast(ResolvedCast* expr) {
    generateExpression(expr->expr);
    current_result_->writeOpcode(sblr::Opcode::EXPR_CAST);
    // CAST payload: try_cast flag + target type + modifiers + format
    // See docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md
    current_result_->writeByte(0);  // try_cast = false
    generateDataType(expr->target_type);
    current_result_->writeByte(static_cast<uint8_t>(expr->format));
}

void BytecodeGeneratorV2::generateCase(ResolvedCase* expr) {
    current_result_->writeOpcode(sblr::Opcode::CASE_WHEN);

    // Write operand if simple CASE
    if (expr->operand) {
        current_result_->writeByte(1);  // Has operand
        generateExpression(expr->operand);
    } else {
        current_result_->writeByte(0);  // Searched CASE
    }

    // Write number of WHEN clauses
    current_result_->writeListCount(static_cast<uint64_t>(expr->when_clauses.size()));

    // Generate each WHEN clause
    for (const auto& when : expr->when_clauses) {
        generateExpression(when.when_expr);
        generateExpression(when.then_expr);
    }

    // Generate ELSE clause
    if (expr->else_expr) {
        current_result_->writeByte(1);  // Has ELSE
        generateExpression(expr->else_expr);
    } else {
        current_result_->writeByte(0);  // No ELSE
    }
}

void BytecodeGeneratorV2::generateSubquery(ResolvedSubqueryExpr* expr) {
    if (expr->is_scalar) {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_SUBQUERY_SCALAR);
    }

    if (expr->subquery) {
        generateStatement(expr->subquery);
    }

    current_result_->writeExtendedOpcode(
        sblr::ExtendedOpcode::EXT_SUBQUERY_END);
}

void BytecodeGeneratorV2::generateExists(ResolvedExistsExpr* expr) {
    current_result_->writeExtendedOpcode(
        sblr::ExtendedOpcode::EXT_SUBQUERY_EXISTS);

    if (expr->negated) {
        current_result_->writeByte(1);
    } else {
        current_result_->writeByte(0);
    }

    if (expr->subquery) {
        generateStatement(expr->subquery);
    }

    current_result_->writeExtendedOpcode(
        sblr::ExtendedOpcode::EXT_SUBQUERY_END);
}

void BytecodeGeneratorV2::generateIn(ResolvedInExpr* expr) {
    generateExpression(expr->expr);

    if (expr->has_subquery && expr->subquery) {
        if (expr->negated) {
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SUBQUERY_NOT_IN);
        } else {
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SUBQUERY_IN);
        }
        generateStatement(expr->subquery);
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_SUBQUERY_END);
    } else {
        // IN with value list
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_IN_LIST);
        current_result_->writeByte(expr->negated ? 1 : 0);
        current_result_->writeListCount(static_cast<uint64_t>(expr->values.size()));
        for (auto* val : expr->values) {
            generateExpression(val);
        }
    }
}

void BytecodeGeneratorV2::generateBetween(ResolvedBetweenExpr* expr) {
    // BETWEEN is equivalent to: expr >= low AND expr <= high
    // For NOT BETWEEN: expr < low OR expr > high

    generateExpression(expr->expr);
    generateExpression(expr->low);
    current_result_->writeOpcode(sblr::Opcode::EXPR_GE);

    generateExpression(expr->expr);
    generateExpression(expr->high);
    current_result_->writeOpcode(sblr::Opcode::EXPR_LE);

    current_result_->writeOpcode(sblr::Opcode::EXPR_AND);

    if (expr->negated) {
        // NOT result
        current_result_->writeOpcode(sblr::Opcode::LITERAL_INT32);
        current_result_->writeInt32(0);
        current_result_->writeOpcode(sblr::Opcode::EXPR_EQ);
    }
}

void BytecodeGeneratorV2::generateLike(ResolvedLikeExpr* expr) {
    if (expr->match_kind == LikeMatchKind::SIMILAR) {
        generateExpression(expr->expr);
        generateExpression(expr->pattern);

        if (expr->escape) {
            current_result_->addWarning("SIMILAR TO ESCAPE is not supported; ignoring ESCAPE clause");
        }

        if (expr->negated) {
            current_result_->writeExtendedOpcode(expr->case_insensitive ?
                                                    sblr::ExtendedOpcode::EXT_REGEX_NOT_MATCH_CI :
                                                    sblr::ExtendedOpcode::EXT_REGEX_NOT_MATCH);
        } else {
            current_result_->writeExtendedOpcode(expr->case_insensitive ?
                                                    sblr::ExtendedOpcode::EXT_REGEX_MATCH_CI :
                                                    sblr::ExtendedOpcode::EXT_REGEX_MATCH);
        }
        return;
    }

    generateExpression(expr->expr);
    generateExpression(expr->pattern);

    if (expr->escape) {
        generateExpression(expr->escape);
        if (expr->case_insensitive) {
            current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ILIKE_ESCAPE);
        } else {
            current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_LIKE_ESCAPE);
        }
    } else {
        if (expr->case_insensitive) {
            current_result_->writeOpcode(sblr::Opcode::EXPR_ILIKE);
        } else {
            current_result_->writeOpcode(sblr::Opcode::EXPR_LIKE);
        }
    }

    // Handle NOT LIKE
    if (expr->negated) {
        current_result_->writeOpcode(sblr::Opcode::LITERAL_INT32);
        current_result_->writeInt32(0);
        current_result_->writeOpcode(sblr::Opcode::EXPR_EQ);
    }
}

void BytecodeGeneratorV2::generateIsNull(ResolvedIsNullExpr* expr) {
    generateExpression(expr->expr);

    // IS NULL: NULL-safe equality against NULL literal
    current_result_->writeOpcode(sblr::Opcode::LITERAL_NULL);
    current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ);

    // Handle IS NOT NULL
    if (expr->negated) {
        current_result_->writeOpcode(sblr::Opcode::LITERAL_INT32);
        current_result_->writeInt32(0);
        current_result_->writeOpcode(sblr::Opcode::EXPR_EQ);
    }
}

void BytecodeGeneratorV2::generateArray(ResolvedArrayExpr* expr) {
    if (expr->has_subquery && expr->subquery) {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_SUBQUERY_ARRAY);
        generateStatement(expr->subquery);
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_SUBQUERY_END);
    } else {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_ARRAY_CONSTRUCT);
        current_result_->writeListCount(static_cast<uint64_t>(expr->elements.size()));
        for (auto* elem : expr->elements) {
            generateExpression(elem);
        }
    }
}

void BytecodeGeneratorV2::generateExtract(ResolvedExtractExpr* expr) {
    if (!expr) {
        current_result_->addError("EXTRACT expression is null");
        return;
    }

    // Emit selector arguments first (postfix), then source.
    for (auto* arg : expr->selector.args) {
        generateExpression(arg);
    }
    generateExpression(expr->source);

    if (expr->selector.args.size() > std::numeric_limits<uint8_t>::max()) {
        current_result_->addError("EXTRACT argument count exceeds byte limit");
    }

    current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_EXTRACT);
    current_result_->writeByte(expr->selector.field_id);
    current_result_->writeByte(static_cast<uint8_t>(expr->selector.args.size()));
}

void BytecodeGeneratorV2::generateAlterElement(ResolvedAlterElementExpr* expr) {
    if (!expr) {
        current_result_->addError("ALTER_ELEMENT expression is null");
        return;
    }

    // Emit selector arguments first, then source, then new value.
    for (auto* arg : expr->selector.args) {
        generateExpression(arg);
    }
    generateExpression(expr->source);
    generateExpression(expr->new_value);

    if (expr->selector.args.size() > std::numeric_limits<uint8_t>::max()) {
        current_result_->addError("ALTER_ELEMENT argument count exceeds byte limit");
    }

    current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_ALTER_ELEMENT);
    current_result_->writeByte(expr->selector.field_id);
    current_result_->writeByte(static_cast<uint8_t>(expr->selector.args.size()));
}

// =============================================================================
// Clause Generation
// =============================================================================

void BytecodeGeneratorV2::generateSelectList(const std::vector<ResolvedSelectItem>& items) {
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeListCount(static_cast<uint64_t>(items.size()));

    for (const auto& item : items) {
        switch (item.item_type) {
            case ResolvedSelectItem::ItemType::STAR:
                current_result_->writeOpcode(sblr::Opcode::SELECT_STAR);
                break;

            case ResolvedSelectItem::ItemType::TABLE_STAR:
                current_result_->writeExtendedOpcode(
                    sblr::ExtendedOpcode::EXT_SELECT_TABLE_STAR);
                writeTableRefPayload(item.table_uuid, StringPool::INVALID_ID, false,
                                     StringPool::INVALID_ID);
                break;

            case ResolvedSelectItem::ItemType::EXPRESSION:
                generateExpression(item.expr);
                if (item.has_alias) {
                    writeStringId(item.alias);
                } else {
                    current_result_->writeString(std::string());
                }
                break;
        }
    }

    current_result_->writeOpcode(sblr::Opcode::END_LIST);
}

void BytecodeGeneratorV2::generateFromClause(const std::vector<ResolvedTableRef*>& tables,
                                             const std::vector<ResolvedJoin*>& joins) {
    // Write table references (compact stream list; Appendix_A_SBLR_BYTECODE.md).
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeListCount(static_cast<uint64_t>(tables.size()));

    for (auto* table : tables) {
        current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
        writeTableRefPayload(*table);
    }

    current_result_->writeOpcode(sblr::Opcode::END_LIST);

    // Write joins
    for (auto* join : joins) {
        current_result_->writeOpcode(sblr::Opcode::JOIN_TYPE);
        current_result_->writeByte(static_cast<uint8_t>(join->join_type));

        // Write right table
        if (join->right) {
            current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
            writeTableRefPayload(*join->right);
        }

        // Write join condition
        if (join->on_condition) {
            current_result_->writeOpcode(sblr::Opcode::JOIN_CONDITION);
            generateExpression(join->on_condition);
        }
    }
}

void BytecodeGeneratorV2::generateWhereClause(ResolvedExpression* where) {
    current_result_->writeOpcode(sblr::Opcode::WHERE_CLAUSE);
    generateExpression(where);
}

void BytecodeGeneratorV2::generateGroupByClause(const std::vector<ResolvedExpression*>& group_by) {
    current_result_->writeOpcode(sblr::Opcode::GROUP_BY);
    current_result_->writeListCount(static_cast<uint64_t>(group_by.size()));

    for (auto* expr : group_by) {
        generateExpression(expr);
    }
}

void BytecodeGeneratorV2::generateHavingClause(ResolvedExpression* having) {
    current_result_->writeOpcode(sblr::Opcode::HAVING);
    generateExpression(having);
}

void BytecodeGeneratorV2::generateOrderByClause(const std::vector<ResolvedOrderByItem*>& order_by) {
    current_result_->writeOpcode(sblr::Opcode::ORDER_BY);
    current_result_->writeListCount(static_cast<uint64_t>(order_by.size()));

    for (auto* item : order_by) {
        current_result_->writeOpcode(sblr::Opcode::SORT_KEY);
        generateExpression(item->expr);

        if (item->ascending) {
            current_result_->writeOpcode(sblr::Opcode::SORT_ASC);
        } else {
            current_result_->writeOpcode(sblr::Opcode::SORT_DESC);
        }

        if (item->nulls_first) {
            current_result_->writeOpcode(sblr::Opcode::NULLS_FIRST);
        } else if (item->nulls_last) {
            current_result_->writeOpcode(sblr::Opcode::NULLS_LAST);
        }
    }
}

void BytecodeGeneratorV2::generateLimitOffset(ResolvedExpression* limit, ResolvedExpression* offset) {
    if (limit) {
        current_result_->writeOpcode(sblr::Opcode::LIMIT);
        generateExpression(limit);
    }

    if (offset) {
        current_result_->writeOpcode(sblr::Opcode::OFFSET);
        generateExpression(offset);
    }
}

// =============================================================================
// Type Generation
// =============================================================================

void BytecodeGeneratorV2::generateDataType(const ResolvedType& type) {
    // See docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md for SBLR type encoding.
    if (type.data_type == DataType::INT128 || type.data_type == DataType::UINT128) {
        current_result_->writeOpcode(sblr::Opcode::EXTENDED_OPCODE);
        current_result_->writeExtendedOpcode(
            type.data_type == DataType::INT128
                ? sblr::ExtendedOpcode::EXT_TYPE_INT128
                : sblr::ExtendedOpcode::EXT_TYPE_UINT128);
        return;
    }

    current_result_->writeOpcode(dataTypeToOpcode(type.data_type));

    // Write type modifiers based on type
    switch (type.data_type) {
        case DataType::VARCHAR:
        case DataType::CHAR:
            if (type.length) {
                current_result_->writeInt32(*type.length);
            } else {
                current_result_->writeInt32(255);  // Default length
            }
            break;

        case DataType::DECIMAL:
            if (type.precision) {
                current_result_->writeInt32(*type.precision);
            } else {
                current_result_->writeInt32(18);  // Default precision
            }
            if (type.scale) {
                current_result_->writeInt32(*type.scale);
            } else {
                current_result_->writeInt32(0);  // Default scale
            }
            break;

        default:
            // No additional modifiers needed
            break;
    }
}

sblr::Opcode BytecodeGeneratorV2::dataTypeToOpcode(DataType type) {
    switch (type) {
        case DataType::INT8: return sblr::Opcode::TYPE_INT8;
        case DataType::INT16: return sblr::Opcode::TYPE_INT16;
        case DataType::INT32: return sblr::Opcode::TYPE_INTEGER;
        case DataType::INT64: return sblr::Opcode::TYPE_BIGINT;
        case DataType::FLOAT32: return sblr::Opcode::TYPE_FLOAT32;
        case DataType::FLOAT64: return sblr::Opcode::TYPE_DOUBLE;
        case DataType::DECIMAL: return sblr::Opcode::TYPE_DECIMAL;
        case DataType::BOOLEAN: return sblr::Opcode::TYPE_BOOLEAN;
        case DataType::VARCHAR: return sblr::Opcode::TYPE_VARCHAR;
        case DataType::CHAR: return sblr::Opcode::TYPE_CHAR;
        case DataType::TEXT: return sblr::Opcode::TYPE_TEXT;
        case DataType::DATE: return sblr::Opcode::TYPE_DATE;
        case DataType::TIME: return sblr::Opcode::TYPE_TIME;
        case DataType::TIMESTAMP: return sblr::Opcode::TYPE_TIMESTAMP;
        case DataType::UUID: return sblr::Opcode::TYPE_UUID;
        case DataType::BINARY: return sblr::Opcode::TYPE_BINARY;
        case DataType::VARBINARY: return sblr::Opcode::TYPE_VARBINARY;
        case DataType::BLOB: return sblr::Opcode::TYPE_BLOB;
        case DataType::JSON: return sblr::Opcode::TYPE_JSON;
        default: return sblr::Opcode::TYPE_VARCHAR;
    }
}

void BytecodeGeneratorV2::writeTypeRef(const ResolvedType& type) {
    if (type.is_domain && type.domain_id != ID{}) {
        current_result_->writeByte(1);
        current_result_->writeUUID(type.domain_id);
        return;
    }
    current_result_->writeByte(0);
    generateDataType(type);
}

// =============================================================================
// Utility Methods
// =============================================================================

std::string_view BytecodeGeneratorV2::getString(StringPool::StringId id) const {
    return string_pool_.get(id);
}

void BytecodeGeneratorV2::writeStringId(StringPool::StringId id) {
    std::string_view str = getString(id);
    current_result_->writeString(std::string(str));
}

void BytecodeGeneratorV2::writeString16(std::string_view str) {
    if (str.size() > std::numeric_limits<uint16_t>::max()) {
        current_result_->addError("String length exceeds 16-bit limit");
        current_result_->writeInt16(0);
        return;
    }

    current_result_->writeInt16(static_cast<uint16_t>(str.size()));
    for (unsigned char ch : str) {
        current_result_->writeByte(static_cast<uint8_t>(ch));
    }
}

void BytecodeGeneratorV2::writeObjectPath(const SchemaPath& path) {
    current_result_->writeByte(static_cast<uint8_t>(path.type));
    current_result_->writeByte(path.no_search_path ? 1 : 0);
    if (path.components.size() > std::numeric_limits<uint8_t>::max()) {
        current_result_->addError("Object path has too many components");
        current_result_->writeByte(0);
        return;
    }
    current_result_->writeByte(static_cast<uint8_t>(path.components.size()));
    for (auto component : path.components) {
        writeString16(getString(component));
    }
}

void BytecodeGeneratorV2::writeTableRefPayload(const ResolvedTableRef& table_ref) {
    writeTableRefPayload(table_ref.table_uuid, table_ref.name, table_ref.has_alias, table_ref.alias);
}

void BytecodeGeneratorV2::writeTableRefPayload(const core::ID& table_uuid,
                                               StringPool::StringId name,
                                               bool has_alias,
                                               StringPool::StringId alias) {
    const bool has_uuid = !isZeroUuid(table_uuid);
    current_result_->writeByte(has_uuid ? 1 : 0);
    if (has_uuid) {
        current_result_->writeUUID(table_uuid);
    } else if (name != StringPool::INVALID_ID) {
        writeStringId(name);
    } else {
        current_result_->addError("TABLE_REF missing name and UUID");
        current_result_->writeString(std::string());
    }

    if (has_alias && alias != StringPool::INVALID_ID) {
        writeStringId(alias);
    } else {
        current_result_->writeString(std::string());
    }
}

sblr::Opcode BytecodeGeneratorV2::binaryOpToOpcode(BinaryOp op) {
    switch (op) {
        case BinaryOp::ADD: return sblr::Opcode::EXPR_ADD;
        case BinaryOp::SUB: return sblr::Opcode::EXPR_SUBTRACT;
        case BinaryOp::MUL: return sblr::Opcode::EXPR_MULTIPLY;
        case BinaryOp::DIV: return sblr::Opcode::EXPR_DIVIDE;
        case BinaryOp::MOD: return sblr::Opcode::EXPR_MODULO;
        case BinaryOp::EQ: return sblr::Opcode::EXPR_EQ;
        case BinaryOp::NE: return sblr::Opcode::EXPR_NE;
        case BinaryOp::LT: return sblr::Opcode::EXPR_LT;
        case BinaryOp::GT: return sblr::Opcode::EXPR_GT;
        case BinaryOp::LE: return sblr::Opcode::EXPR_LE;
        case BinaryOp::GE: return sblr::Opcode::EXPR_GE;
        case BinaryOp::AND: return sblr::Opcode::EXPR_AND;
        case BinaryOp::OR: return sblr::Opcode::EXPR_OR;
        case BinaryOp::JSON_EXTRACT: return sblr::Opcode::JSON_ARROW;
        case BinaryOp::JSON_EXTRACT_TEXT: return sblr::Opcode::JSON_DOUBLE_ARROW;
        case BinaryOp::JSON_HASH_EXTRACT: return sblr::Opcode::JSON_HASH_ARROW;
        case BinaryOp::JSON_HASH_EXTRACT_TEXT: return sblr::Opcode::JSON_HASH_DOUBLE_ARROW;
        default: return sblr::Opcode::EXPR_ADD;  // Default
    }
}

// =============================================================================
// Optimization Passes
// =============================================================================

ResolvedLiteral* BytecodeGeneratorV2::tryConstantFold(ResolvedExpression* expr) {
    // Handle unary NEGATE with literal operand
    if (auto* unary = dynamic_cast<ResolvedUnaryExpr*>(expr)) {
        if (unary->op == UnaryOp::NEGATE) {
            auto* operand_lit = dynamic_cast<ResolvedLiteral*>(unary->operand);
            if (operand_lit && !operand_lit->is_null) {
                static ResolvedLiteral negated;
                negated.is_null = false;

                if (operand_lit->literal_type == LiteralType::INTEGER) {
                    negated.literal_type = LiteralType::INTEGER;
                    negated.type.data_type = operand_lit->type.data_type;
                    negated.int_value = -operand_lit->int_value;
                    return &negated;
                } else if (operand_lit->literal_type == LiteralType::FLOAT) {
                    negated.literal_type = LiteralType::FLOAT;
                    negated.type.data_type = DataType::FLOAT64;
                    negated.float_value = -operand_lit->float_value;
                    return &negated;
                }
            }
        }
        return nullptr;
    }

    // Fold binary expressions with two literal operands
    auto* binary = dynamic_cast<ResolvedBinaryExpr*>(expr);
    if (!binary) return nullptr;

    auto* left_lit = dynamic_cast<ResolvedLiteral*>(binary->left);
    auto* right_lit = dynamic_cast<ResolvedLiteral*>(binary->right);

    if (!left_lit || !right_lit) return nullptr;
    if (left_lit->is_null || right_lit->is_null) return nullptr;

    // Only fold integer arithmetic for now
    if (left_lit->literal_type != LiteralType::INTEGER ||
        right_lit->literal_type != LiteralType::INTEGER) {
        return nullptr;
    }

    // Note: This is a simple implementation that doesn't use the arena
    // In production, we'd allocate from the arena
    static ResolvedLiteral folded;
    folded.literal_type = LiteralType::INTEGER;
    folded.type.data_type = DataType::INT64;
    folded.is_null = false;

    switch (binary->op) {
        case BinaryOp::ADD:
            folded.int_value = left_lit->int_value + right_lit->int_value;
            break;
        case BinaryOp::SUB:
            folded.int_value = left_lit->int_value - right_lit->int_value;
            break;
        case BinaryOp::MUL:
            folded.int_value = left_lit->int_value * right_lit->int_value;
            break;
        case BinaryOp::DIV:
            if (right_lit->int_value == 0) return nullptr;  // Division by zero
            folded.int_value = left_lit->int_value / right_lit->int_value;
            break;
        case BinaryOp::MOD:
            if (right_lit->int_value == 0) return nullptr;
            folded.int_value = left_lit->int_value % right_lit->int_value;
            break;
        default:
            return nullptr;  // Other ops not folded
    }

    return &folded;
}

bool BytecodeGeneratorV2::isConstant(ResolvedExpression* expr) {
    if (dynamic_cast<ResolvedLiteral*>(expr)) {
        return true;
    }

    if (auto* binary = dynamic_cast<ResolvedBinaryExpr*>(expr)) {
        return isConstant(binary->left) && isConstant(binary->right);
    }

    if (auto* unary = dynamic_cast<ResolvedUnaryExpr*>(expr)) {
        return isConstant(unary->operand);
    }

    return false;
}

// =============================================================================
// Convenience Functions
// =============================================================================

BytecodeResultV2 generateBytecode(
    std::string_view sql,
    core::CatalogManager& catalog,
    StringPool& string_pool) {

    // Parse
    Parser parser(sql);
    auto parse_result = parser.parseStatement();

    if (!parse_result.success()) {
        BytecodeResultV2 result;
        for (const auto& err : parse_result.errors()) {
            result.addError("Parse error: " + err.message);
        }
        return result;
    }

    // Semantic analysis
    SemanticAnalyzerV2 analyzer(catalog, string_pool);
    auto sem_result = analyzer.analyze(parse_result.statement());

    if (!sem_result.success()) {
        BytecodeResultV2 result;
        for (const auto& err : sem_result.errors()) {
            result.addError("Semantic error: " + err.message);
        }
        return result;
    }

    // Generate bytecode
    BytecodeGeneratorV2 generator(string_pool);
    return generator.generate(sem_result.statement());
}

// =============================================================================
// Bytecode Disassembler
// =============================================================================

std::string BytecodeDisassemblerV2::disassemble(const std::vector<uint8_t>& bytecode) {
    std::ostringstream out;
    size_t offset = 0;

    auto canRead = [&](size_t count) {
        return count <= bytecode.size() && offset <= bytecode.size() - count;
    };
    auto readByte = [&]() -> uint8_t {
        if (!canRead(1)) {
            offset = bytecode.size();
            return 0;
        }
        return bytecode[offset++];
    };
    auto readInt16 = [&]() -> uint16_t {
        if (!canRead(2)) {
            offset = bytecode.size();
            return 0;
        }
        uint16_t val = sblr::readInt16(&bytecode[offset]);
        offset += 2;
        return val;
    };
    auto readInt32 = [&]() -> uint32_t {
        if (!canRead(4)) {
            offset = bytecode.size();
            return 0;
        }
        uint32_t val = sblr::readInt32(&bytecode[offset]);
        offset += 4;
        return val;
    };
    auto readString = [&]() -> std::string {
        uint32_t len = readInt32();
        if (len == 0) {
            return std::string();
        }
        if (!canRead(len)) {
            offset = bytecode.size();
            return std::string();
        }
        std::string out_str(reinterpret_cast<const char*>(&bytecode[offset]), len);
        offset += len;
        return out_str;
    };

    auto disassembleTransactionPayload = [&]() {
        uint16_t flags = readInt16();
        readByte();  // conflict_action

        if (flags & sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE) {
            readInt32();
        }
        if (flags & sblr::TransactionFlags::HAS_AUTOCOMMIT) {
            readByte();
        }
        if (flags & sblr::TransactionFlags::HAS_ISOLATION) {
            readByte();
        }
        if (flags & sblr::TransactionFlags::HAS_READ_COMMITTED_MODE) {
            readByte();
        }
        if (flags & sblr::TransactionFlags::HAS_ACCESS_MODE) {
            readByte();
        }
        if (flags & sblr::TransactionFlags::HAS_DEFERRABLE) {
            readByte();
        }
        if (flags & sblr::TransactionFlags::HAS_WAIT_MODE) {
            readByte();
        }
        if (flags & sblr::TransactionFlags::HAS_LOCK_TIMEOUT) {
            readInt32();
        }
        if (flags & sblr::TransactionFlags::HAS_RESERVATIONS) {
            auto list_op = static_cast<sblr::Opcode>(readByte());
            if (list_op == sblr::Opcode::BEGIN_LIST) {
                uint32_t count = readInt32();
                out << "BEGIN_LIST " << std::dec << count << "\n";
                for (uint32_t i = 0; i < count; ++i) {
                    auto item_op = static_cast<sblr::Opcode>(readByte());
                    if (item_op == sblr::Opcode::TABLE_REF) {
                        std::string name = readString();
                        out << "TABLE_REF \"" << name << "\"\n";
                        readByte();  // lock_mode
                        readByte();  // for_write
                    } else {
                        out << "OPCODE_" << std::hex << static_cast<int>(item_op) << "\n";
                    }
                }
                auto end_op = static_cast<sblr::Opcode>(readByte());
                if (end_op == sblr::Opcode::END_LIST) {
                    out << "END_LIST\n";
                } else {
                    out << "OPCODE_" << std::hex << static_cast<int>(end_op) << "\n";
                }
            }
        }
    };

    while (offset < bytecode.size()) {
        out << std::hex << std::setw(4) << std::setfill('0') << offset << ": ";

        uint8_t op = readByte();
        auto opcode = static_cast<sblr::Opcode>(op);

        switch (opcode) {
            case sblr::Opcode::END:
                out << "END\n";
                break;
            case sblr::Opcode::VERSION:
                out << "VERSION " << static_cast<int>(readByte()) << "\n";
                break;
            case sblr::Opcode::SELECT:
                out << "SELECT\n";
                break;
            case sblr::Opcode::INSERT:
                out << "INSERT\n";
                break;
            case sblr::Opcode::UPDATE:
                out << "UPDATE\n";
                break;
            case sblr::Opcode::DELETE:
                out << "DELETE\n";
                break;
            case sblr::Opcode::CREATE_TABLE:
                out << "CREATE_TABLE\n";
                break;
            case sblr::Opcode::DROP_TABLE:
                out << "DROP_TABLE\n";
                break;
            case sblr::Opcode::START_TRANSACTION:
                out << "START_TRANSACTION\n";
                disassembleTransactionPayload();
                break;
            case sblr::Opcode::SET_TRANSACTION:
                out << "SET_TRANSACTION\n";
                disassembleTransactionPayload();
                break;
            case sblr::Opcode::COMMIT:
                out << "COMMIT\n";
                readByte();
                break;
            case sblr::Opcode::ROLLBACK:
                out << "ROLLBACK\n";
                readByte();
                break;
            case sblr::Opcode::TABLE_REF:
                out << "TABLE_REF \"" << readString() << "\"\n";
                break;
            case sblr::Opcode::BEGIN_LIST:
                out << "BEGIN_LIST\n";
                readInt32();
                break;
            case sblr::Opcode::END_LIST:
                out << "END_LIST\n";
                break;
            case sblr::Opcode::LITERAL_INT32:
                if (offset + 4 <= bytecode.size()) {
                    int32_t val = sblr::readInt32(&bytecode[offset]);
                    offset += 4;
                    out << "LITERAL_INT32 " << std::dec << val << "\n";
                }
                break;
            case sblr::Opcode::LITERAL_INT64:
                if (offset + 8 <= bytecode.size()) {
                    int64_t val = static_cast<int64_t>(sblr::readInt64(&bytecode[offset]));
                    offset += 8;
                    out << "LITERAL_INT64 " << std::dec << val << "\n";
                }
                break;
            case sblr::Opcode::LITERAL_NULL:
                out << "LITERAL_NULL\n";
                break;
            case sblr::Opcode::EXPR_ADD:
                out << "ADD\n";
                break;
            case sblr::Opcode::EXPR_SUBTRACT:
                out << "SUBTRACT\n";
                break;
            case sblr::Opcode::EXPR_MULTIPLY:
                out << "MULTIPLY\n";
                break;
            case sblr::Opcode::EXPR_DIVIDE:
                out << "DIVIDE\n";
                break;
            case sblr::Opcode::EXPR_EQ:
                out << "EQ\n";
                break;
            case sblr::Opcode::EXPR_NE:
                out << "NE\n";
                break;
            case sblr::Opcode::EXPR_AND:
                out << "AND\n";
                break;
            case sblr::Opcode::EXPR_OR:
                out << "OR\n";
                break;
            default:
                out << "OPCODE_" << std::hex << static_cast<int>(op) << "\n";
                break;
        }
    }

    return out.str();
}

} // namespace scratchbird::parser::v2
