/**
 * ScratchBird SBLR v2.0 - Bytecode Generator Implementation
 *
 * Generates SBLR bytecode from resolved AST nodes.
 */

#include "scratchbird/sblr/bytecode_generator_v2.h"
#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include <cstring>
#include <sstream>
#include <cmath>
#include <iostream>
#include <limits>

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

    // Generate select list (V1-compatible format: no flags byte before BEGIN_LIST)
    generateSelectListV1Compatible(stmt->select_list);

    // Generate FROM clause (TABLE_REF or empty TABLE_REF for no-FROM)
    generateFromClauseV1Compatible(stmt->from_tables, stmt->joins);

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

void BytecodeGeneratorV2::generateSelectListV1Compatible(const std::vector<ResolvedSelectItem>& items) {
    // V1-compatible format: BEGIN_LIST, count, items, END_LIST
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeInt32(static_cast<uint32_t>(items.size()));

    for (const auto& item : items) {
        switch (item.item_type) {
            case ResolvedSelectItem::ItemType::STAR:
                current_result_->writeOpcode(sblr::Opcode::SELECT_STAR);
                break;

            case ResolvedSelectItem::ItemType::TABLE_STAR:
                current_result_->writeExtendedOpcode(
                    sblr::ExtendedOpcode::EXT_SELECT_TABLE_STAR);
                current_result_->writeUUID(item.table_uuid);
                break;

            case ResolvedSelectItem::ItemType::EXPRESSION:
                generateExpression(item.expr);
                // Write alias if present
                if (item.has_alias) {
                    current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
                    current_result_->writeString("");  // No qualifier
                    writeStringId(item.alias);
                }
                break;
        }
    }

    current_result_->writeOpcode(sblr::Opcode::END_LIST);
}

void BytecodeGeneratorV2::generateFromClauseV1Compatible(const std::vector<ResolvedTableRef*>& tables,
                                                         const std::vector<ResolvedJoin*>& joins) {
    // V1-compatible format: single TABLE_REF with table name string
    // For no-FROM (constant expression SELECT), use empty string TABLE_REF
    if (tables.empty()) {
        // No FROM clause - constant expression SELECT (e.g., SELECT 1)
        current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
        current_result_->writeString("");  // Empty string indicates no table
    } else if (tables.size() == 1 && joins.empty()) {
        // Simple single table FROM clause - use alias if present, otherwise table name
        current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
        if (tables[0]->has_alias) {
            writeStringId(tables[0]->alias);
        } else {
            // Write table name for V1 bytecode compatibility
            // Note: ResolvedTableRef.name is populated by semantic analyzer for this purpose
            writeStringId(tables[0]->name);
        }
    } else {
        // Multiple tables or joins - use original FROM clause generation
        generateFromClause(tables, joins);
    }
}

void BytecodeGeneratorV2::generateInsert(ResolvedInsertStmt* stmt) {
    // Generate v1-compatible bytecode format
    // Format: INSERT, TABLE_REF, table_name, BEGIN_LIST, col_count, [COLUMN_REF, qualifier, name]*, END_LIST,
    //         row_count, [BEGIN_LIST, val_count, values..., END_LIST]*, [RETURNING]

    current_result_->writeOpcode(sblr::Opcode::INSERT);

    // Write table name with TABLE_REF opcode
    current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
    writeStringId(stmt->target_table.name);

    // Write column list
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeInt32(static_cast<uint32_t>(stmt->target_column_indexes.size()));

    for (uint32_t col_idx : stmt->target_column_indexes) {
        if (col_idx < stmt->target_table.columns.size()) {
            current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
            // Note: INSERT executor expects only column name, no qualifier
            // (unlike CREATE TABLE which reads qualifier + name)
            writeStringId(stmt->target_table.columns[col_idx].name);
        }
    }

    current_result_->writeOpcode(sblr::Opcode::END_LIST);

    // Write values - executor expects BEGIN_LIST immediately after column list END_LIST
    // NOTE: The v1 bytecode generator writes row_count before rows, but executor doesn't read it.
    // For compatibility, we write the first row's values directly without row_count prefix.
    if (stmt->source == ResolvedInsertStmt::Source::VALUES && !stmt->values_rows.empty()) {
        // Write first row only (single-row INSERT compatible with executor)
        const auto& row = stmt->values_rows[0];
        current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
        current_result_->writeInt32(static_cast<uint32_t>(row.size()));

        for (auto* expr : row) {
            generateExpression(expr);
        }

        current_result_->writeOpcode(sblr::Opcode::END_LIST);

        // For multi-row INSERT, log warning (not supported by executor yet)
        if (stmt->values_rows.size() > 1) {
            current_result_->addWarning("Multi-row INSERT not fully supported; only first row inserted");
        }
    } else if (stmt->source == ResolvedInsertStmt::Source::SELECT) {
        // INSERT ... SELECT not yet fully supported with v1 executor
        current_result_->addError("INSERT ... SELECT not supported in v1 bytecode format");
    } else {
        // DEFAULT VALUES - empty value list
        current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
        current_result_->writeInt32(0);
        current_result_->writeOpcode(sblr::Opcode::END_LIST);
    }

    // Handle RETURNING
    if (!stmt->returning.empty()) {
        current_result_->writeExtendedOpcode(sblr::ExtendedOpcode::EXT_RETURNING);
        current_result_->writeInt32(static_cast<uint32_t>(stmt->returning.size()));
        for (const auto& item : stmt->returning) {
            if (item.alias != StringPool::INVALID_ID) {
                writeStringId(item.alias);
            } else if (auto* col_ref = dynamic_cast<ResolvedColumnRef*>(item.expr)) {
                writeStringId(col_ref->column_name);
            } else {
                current_result_->writeString("?column?");
            }
        }
    }
}

void BytecodeGeneratorV2::generateUpdate(ResolvedUpdateStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::UPDATE);

    // Write target table UUID
    current_result_->writeUUID(stmt->target_table.table_uuid);

    // Write assignment count
    current_result_->writeInt32(static_cast<uint32_t>(stmt->assignments.size()));

    // Write each assignment: column_index, expression
    for (const auto& [col_idx, expr] : stmt->assignments) {
        current_result_->writeOpcode(sblr::Opcode::ASSIGNMENT);
        current_result_->writeInt32(col_idx);
        generateExpression(expr);
    }

    // Generate FROM clause if present
    if (!stmt->from_tables.empty()) {
        generateFromClause(stmt->from_tables, stmt->joins);
    }

    // Generate WHERE clause
    if (stmt->where) {
        generateWhereClause(stmt->where);
    }

    // Handle RETURNING
    if (!stmt->returning.empty()) {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_RETURNING);
        generateSelectList(stmt->returning);
    }
}

void BytecodeGeneratorV2::generateDelete(ResolvedDeleteStmt* stmt) {
    current_result_->writeOpcode(sblr::Opcode::DELETE);

    // Write target table UUID
    current_result_->writeUUID(stmt->target_table.table_uuid);

    // Generate USING clause if present
    if (!stmt->using_tables.empty()) {
        generateFromClause(stmt->using_tables, stmt->using_joins);
    }

    // Generate WHERE clause
    if (stmt->where) {
        generateWhereClause(stmt->where);
    }

    // Handle RETURNING
    if (!stmt->returning.empty()) {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_RETURNING);
        generateSelectList(stmt->returning);
    }
}

// =============================================================================
// DDL Statement Generation
// =============================================================================

void BytecodeGeneratorV2::generateCreateTable(ResolvedCreateTableStmt* stmt) {
    // Generate v1-compatible bytecode format that the executor expects
    // Format: CREATE_TABLE, TABLE_REF, table_name, BEGIN_LIST, col_count,
    //         [COLUMN_DEF, COLUMN_REF, qualifier, name, type, constraints...]*,
    //         END_LIST, tablespace_name, [table-level constraints]

    current_result_->writeOpcode(sblr::Opcode::CREATE_TABLE);

    // Write table name with TABLE_REF opcode
    current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
    writeStringId(stmt->table_name);

    // Write BEGIN_LIST opcode for columns
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeInt32(static_cast<uint32_t>(stmt->columns.size()));

    // Write each column definition in v1 format
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

    // Write flags
    uint8_t flags = 0;
    if (stmt->unique) flags |= 0x01;
    if (stmt->if_not_exists) flags |= 0x02;
    if (stmt->concurrent) flags |= 0x04;
    current_result_->writeByte(flags);

    // Write index name
    writeStringId(stmt->index_name);

    // Write table UUID
    current_result_->writeUUID(stmt->table_uuid);

    // Write index method
    writeStringId(stmt->index_method);

    // Write column count and details
    current_result_->writeInt32(static_cast<uint32_t>(stmt->column_indexes.size()));
    for (size_t i = 0; i < stmt->column_indexes.size(); ++i) {
        current_result_->writeInt32(stmt->column_indexes[i]);
        current_result_->writeByte(stmt->column_desc[i] ? 1 : 0);
    }

    // Write WHERE clause for partial index
    if (stmt->where_clause) {
        current_result_->writeByte(1);  // Has WHERE
        generateExpression(stmt->where_clause);
    } else {
        current_result_->writeByte(0);  // No WHERE
    }

    // Write tablespace ID
    current_result_->writeInt16(stmt->tablespace_id);
}

void BytecodeGeneratorV2::generateCreateView(ResolvedCreateViewStmt* stmt) {
    if (stmt->materialized) {
        current_result_->writeOpcode(sblr::Opcode::REFRESH_MATERIALIZED_VIEW);
    } else {
        current_result_->writeOpcode(sblr::Opcode::CREATE_VIEW);
    }

    // Write flags
    uint8_t flags = 0;
    if (stmt->or_replace) flags |= 0x01;
    if (stmt->materialized) flags |= 0x02;
    if (stmt->check_option) flags |= 0x04;
    current_result_->writeByte(flags);

    // Write schema UUID
    current_result_->writeUUID(stmt->schema.schema_uuid);

    // Write view name
    writeStringId(stmt->view_name);

    // Write column names if specified
    current_result_->writeInt32(static_cast<uint32_t>(stmt->column_names.size()));
    for (auto col_name : stmt->column_names) {
        writeStringId(col_name);
    }

    // Write query
    if (stmt->query) {
        generateSelect(stmt->query);
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
}

void BytecodeGeneratorV2::generateCreateDomain(ResolvedCreateDomainStmt* stmt) {
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

    current_result_->writeString(schemaPathToString(stmt->domain_path, string_pool_));
    generateDataType(stmt->base_type);

    current_result_->writeByte(stmt->nullable ? 1 : 0);
    current_result_->writeString(stmt->default_value);

    current_result_->writeInt32(static_cast<uint32_t>(stmt->constraints.size()));
    for (const auto& constraint : stmt->constraints) {
        current_result_->writeByte(static_cast<uint8_t>(constraint.type));
        if (constraint.name != StringPool::INVALID_ID) {
            writeStringId(constraint.name);
        } else {
            current_result_->writeString("");
        }
        current_result_->writeString(constraint.expression);
    }

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
    current_result_->writeInt32(static_cast<uint32_t>(stmt->object_uuids.size()));
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
        current_result_->writeInt32(static_cast<uint32_t>(stmt->table_reservations.size()));
        for (const auto& reservation : stmt->table_reservations) {
            current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
            writeStringId(reservation.table_name);
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
                    current_result_->writeInt32(
                        static_cast<uint32_t>(stmt->table_reservations.size()));
                    for (const auto& reservation : stmt->table_reservations) {
                        current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
                        writeStringId(reservation.table_name);
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

        case SetStmt::SetType::PARSER_VERSION:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SET_PARSER_VERSION);
            current_result_->writeByte(stmt->parser_version);  // 1 or 2
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

        case ShowStmt::ShowType::PARSER_VERSION:
            current_result_->writeExtendedOpcode(
                sblr::ExtendedOpcode::EXT_SHOW_VARIABLE);
            // Write "parser_version" as the variable name
            writeStringId(stmt->variable_name);
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
    current_result_->writeInt32(static_cast<uint32_t>(stmt->table_uuids.size()));
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
    // V1-compatible format: COLUMN_REF followed by column name string only
    // The executor looks up the column by name in the current row context
    current_result_->writeOpcode(sblr::Opcode::COLUMN_REF);
    writeStringId(expr->column.column_name);
}

void BytecodeGeneratorV2::generateBinaryExpr(ResolvedBinaryExpr* expr) {
    // Generate operands first (postfix notation)
    generateExpression(expr->left);
    generateExpression(expr->right);

    // Generate operator
    current_result_->writeOpcode(binaryOpToOpcode(expr->op));
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

    // Check for aggregate functions
    if (expr->function.is_aggregate) {
        std::string_view name = getString(expr->function.function_name);

        if (name == "COUNT") {
            current_result_->writeOpcode(sblr::Opcode::AGG_COUNT);
        } else if (name == "SUM") {
            current_result_->writeOpcode(sblr::Opcode::AGG_SUM);
        } else if (name == "AVG") {
            current_result_->writeOpcode(sblr::Opcode::AGG_AVG);
        } else if (name == "MIN") {
            current_result_->writeOpcode(sblr::Opcode::AGG_MIN);
        } else if (name == "MAX") {
            current_result_->writeOpcode(sblr::Opcode::AGG_MAX);
        } else {
            // Generic function call
            writeStringId(expr->function.function_name);
            current_result_->writeInt32(static_cast<uint32_t>(expr->arguments.size()));
        }
        return;
    }

    // Built-in functions
    std::string_view name = getString(expr->function.function_name);

    // String functions
    if (name == "LENGTH" || name == "LEN") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_LENGTH);
    } else if (name == "UPPER") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_UPPER);
    } else if (name == "LOWER") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_LOWER);
    } else if (name == "TRIM") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_TRIM);
    } else if (name == "SUBSTRING" || name == "SUBSTR") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_SUBSTRING);
    }
    // Date/time functions
    else if (name == "NOW" || name == "CURRENT_TIMESTAMP") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_NOW);
    } else if (name == "CURRENT_DATE") {
        current_result_->writeOpcode(sblr::Opcode::FUNC_CURRENT_DATE);
    }
    // Math functions
    else if (name == "ABS") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ABS);
    } else if (name == "ROUND") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_ROUND);
    } else if (name == "FLOOR") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_FLOOR);
    } else if (name == "CEIL" || name == "CEILING") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_CEIL);
    } else if (name == "SQRT") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_SQRT);
    } else if (name == "POWER" || name == "POW") {
        current_result_->writeExtendedOpcode(
            sblr::ExtendedOpcode::EXT_FUNC_POWER);
    }
    // Null handling
    else if (name == "COALESCE") {
        current_result_->writeOpcode(sblr::Opcode::COALESCE);
        current_result_->writeInt32(static_cast<uint32_t>(expr->arguments.size()));
    } else if (name == "NULLIF") {
        current_result_->writeOpcode(sblr::Opcode::NULLIF);
    }
    // Generic function call
    else {
        writeStringId(expr->function.function_name);
        current_result_->writeInt32(static_cast<uint32_t>(expr->arguments.size()));
    }
}

void BytecodeGeneratorV2::generateCast(ResolvedCast* expr) {
    generateExpression(expr->expr);
    current_result_->writeOpcode(sblr::Opcode::EXPR_CAST);
    generateDataType(expr->target_type);
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
    current_result_->writeInt32(static_cast<uint32_t>(expr->when_clauses.size()));

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
        current_result_->writeInt32(static_cast<uint32_t>(expr->values.size()));
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
    generateExpression(expr->expr);
    generateExpression(expr->pattern);

    if (expr->case_insensitive) {
        current_result_->writeOpcode(sblr::Opcode::EXPR_ILIKE);
    } else {
        current_result_->writeOpcode(sblr::Opcode::EXPR_LIKE);
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

    // IS NULL: compare with NULL using special opcode
    current_result_->writeOpcode(sblr::Opcode::LITERAL_NULL);
    current_result_->writeOpcode(sblr::Opcode::EXPR_EQ);

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
        current_result_->writeInt32(static_cast<uint32_t>(expr->elements.size()));
        for (auto* elem : expr->elements) {
            generateExpression(elem);
        }
    }
}

// =============================================================================
// Clause Generation
// =============================================================================

void BytecodeGeneratorV2::generateSelectList(const std::vector<ResolvedSelectItem>& items) {
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeInt32(static_cast<uint32_t>(items.size()));

    for (const auto& item : items) {
        switch (item.item_type) {
            case ResolvedSelectItem::ItemType::STAR:
                current_result_->writeOpcode(sblr::Opcode::SELECT_STAR);
                break;

            case ResolvedSelectItem::ItemType::TABLE_STAR:
                current_result_->writeOpcode(sblr::Opcode::SELECT_STAR);
                current_result_->writeUUID(item.table_uuid);
                break;

            case ResolvedSelectItem::ItemType::EXPRESSION:
                generateExpression(item.expr);
                if (item.has_alias) {
                    writeStringId(item.alias);
                } else {
                    current_result_->writeInt32(0);  // No alias
                }
                break;
        }
    }

    current_result_->writeOpcode(sblr::Opcode::END_LIST);
}

void BytecodeGeneratorV2::generateFromClause(const std::vector<ResolvedTableRef*>& tables,
                                             const std::vector<ResolvedJoin*>& joins) {
    // Write table references
    current_result_->writeOpcode(sblr::Opcode::BEGIN_LIST);
    current_result_->writeInt32(static_cast<uint32_t>(tables.size()));

    for (auto* table : tables) {
        current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
        current_result_->writeUUID(table->table_uuid);

        if (table->has_alias) {
            writeStringId(table->alias);
        } else {
            current_result_->writeInt32(0);  // No alias
        }
    }

    current_result_->writeOpcode(sblr::Opcode::END_LIST);

    // Write joins
    for (auto* join : joins) {
        current_result_->writeOpcode(sblr::Opcode::JOIN_TYPE);
        current_result_->writeByte(static_cast<uint8_t>(join->join_type));

        // Write right table
        if (join->right) {
            current_result_->writeOpcode(sblr::Opcode::TABLE_REF);
            current_result_->writeUUID(join->right->table_uuid);
            if (join->right->has_alias) {
                writeStringId(join->right->alias);
            } else {
                current_result_->writeInt32(0);
            }
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
    current_result_->writeInt32(static_cast<uint32_t>(group_by.size()));

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
    current_result_->writeInt32(static_cast<uint32_t>(order_by.size()));

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

    while (offset < bytecode.size()) {
        out << std::hex << std::setw(4) << std::setfill('0') << offset << ": ";

        uint8_t op = bytecode[offset++];
        auto opcode = static_cast<sblr::Opcode>(op);

        switch (opcode) {
            case sblr::Opcode::END:
                out << "END\n";
                break;
            case sblr::Opcode::VERSION:
                out << "VERSION " << static_cast<int>(bytecode[offset++]) << "\n";
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
