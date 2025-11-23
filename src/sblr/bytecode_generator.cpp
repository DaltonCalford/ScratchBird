#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/expression_serializer.h"
#include "scratchbird/core/catalog_manager.h"  // LSM Integration: For parseIndexType()
#include "scratchbird/optimizer/query_planner.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace scratchbird
{
    namespace sblr
    {

        BytecodeGenerator::BytecodeGenerator(const parser::StringPool &string_pool,
                                            core::Database *database)
            : string_pool_(string_pool), current_result_(nullptr), database_(database)
        {
        }

        BytecodeGenerator::~BytecodeGenerator() = default;

        BytecodeResult BytecodeGenerator::generate(parser::Statement *stmt)
        {
            BytecodeResult result;
            current_result_ = &result;

            if (!stmt)
            {
                result.addError("Null statement passed to generator");
                current_result_ = nullptr;
                return result;
            }

            // Write version header
            result.writeOpcode(Opcode::VERSION);
            result.writeByte(SBLR_VERSION);

            // Generate bytecode for statement
            stmt->accept(this);

            // Write end marker
            result.writeOpcode(Opcode::END);

            current_result_ = nullptr;
            return result;
        }

        void BytecodeGenerator::writeStringId(parser::StringPool::StringId id)
        {
            std::string_view str = string_pool_.get(id);
            current_result_->writeString(std::string(str));
        }

        void BytecodeGenerator::writeDataType(const parser::TypeName &type)
        {
            switch (type.type)
            {
                case parser::DataType::INT32:
                    current_result_->writeOpcode(Opcode::TYPE_INTEGER);
                    break;
                case parser::DataType::INT64:
                    current_result_->writeOpcode(Opcode::TYPE_BIGINT);
                    break;
                case parser::DataType::FLOAT64:
                    current_result_->writeOpcode(Opcode::TYPE_DOUBLE);
                    break;
                case parser::DataType::VARCHAR:
                    current_result_->writeOpcode(Opcode::TYPE_VARCHAR);
                    current_result_->writeInt32(type.precision);
                    break;
            }
        }

        bool BytecodeGenerator::isActiveCTE(parser::StringPool::StringId name_id) const
        {
            // Phase 2 Wave 2: Check if name is an active CTE
            return active_ctes_.find(name_id) != active_ctes_.end();
        }

        void BytecodeGenerator::generateExpression(parser::Expression *expr)
        {
            if (!expr)
            {
                current_result_->addError("Null expression in bytecode generation");
                return;
            }
            expr->accept(this);
        }

        // ===== Statement Visitors =====

        void BytecodeGenerator::visit(parser::CreateTableStmt *node)
        {
            current_result_->writeOpcode(Opcode::CREATE_TABLE);

            // Write table name
            current_result_->writeOpcode(Opcode::TABLE_REF);
            writeStringId(node->tableName());

            // Write column count with overflow check
            size_t col_count = node->columns().size();
            if (col_count > UINT32_MAX)
            {
                current_result_->addError("Column count exceeds maximum (4 billion)");
                return;
            }

            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(col_count));

            // Write each column definition
            for (auto *col : node->columns())
            {
                if (!col)
                {
                    current_result_->addError("Null column definition in CREATE TABLE");
                    continue;
                }
                col->accept(this);
            }

            current_result_->writeOpcode(Opcode::END_LIST);

            // Write tablespace name (Phase 2 Task 2.3)
            writeStringId(node->tablespace());

            // ALPHA Phase C: Write table-level constraints
            const auto &constraints = node->tableConstraints();
            for (auto *constraint : constraints)
            {
                if (!constraint)
                {
                    continue;
                }

                // Handle ForeignKeyConstraint
                auto *fk_constraint = dynamic_cast<parser::ForeignKeyConstraint *>(constraint);
                if (fk_constraint)
                {
                    // Generate TABLE_FK opcode for table-level FK
                    // Format: TABLE_FK | child_col_count | child_cols... | parent_table | parent_col_count | parent_cols... | on_delete | on_update
                    current_result_->writeOpcode(Opcode::TABLE_FK);

                    // Write child column count and names
                    const auto &child_cols = fk_constraint->childColumns();
                    if (child_cols.size() > UINT8_MAX)
                    {
                        current_result_->addError("Too many child columns in FK constraint (max 255)");
                        continue;
                    }
                    current_result_->writeByte(static_cast<uint8_t>(child_cols.size()));
                    for (auto col_id : child_cols)
                    {
                        writeStringId(col_id);
                    }

                    // Write parent table name
                    writeStringId(fk_constraint->parentTable());

                    // Write parent column count and names
                    const auto &parent_cols = fk_constraint->parentColumns();
                    if (parent_cols.size() > UINT8_MAX)
                    {
                        current_result_->addError("Too many parent columns in FK constraint (max 255)");
                        continue;
                    }
                    current_result_->writeByte(static_cast<uint8_t>(parent_cols.size()));
                    for (auto col_id : parent_cols)
                    {
                        writeStringId(col_id);
                    }

                    // Write ON DELETE action
                    writeStringId(fk_constraint->onDelete());

                    // Write ON UPDATE action
                    writeStringId(fk_constraint->onUpdate());

                    // Write constraint name (optional)
                    writeStringId(fk_constraint->name());

                    // ALPHA Phase 1 - Deferred constraint checking
                    // Write deferrable flags: 1 byte (bit 0 = is_deferrable, bit 1 = initially_deferred)
                    uint8_t deferrable_flags = 0;
                    if (fk_constraint->isDeferrable())
                    {
                        deferrable_flags |= 0x01;
                    }
                    if (fk_constraint->initiallyDeferred())
                    {
                        deferrable_flags |= 0x02;
                    }
                    current_result_->writeByte(deferrable_flags);

                    continue;
                }

                // Handle UniqueConstraint
                auto *unique_constraint = dynamic_cast<parser::UniqueConstraint *>(constraint);
                if (unique_constraint)
                {
                    // Generate UNIQUE_CONSTRAINT opcode for table-level UNIQUE
                    // Format: UNIQUE_CONSTRAINT | col_count | col_names...
                    current_result_->writeOpcode(Opcode::UNIQUE_CONSTRAINT);

                    // Write column count and names
                    const auto &cols = unique_constraint->columns();
                    if (cols.size() > UINT8_MAX)
                    {
                        current_result_->addError("Too many columns in UNIQUE constraint (max 255)");
                        continue;
                    }
                    current_result_->writeByte(static_cast<uint8_t>(cols.size()));
                    for (auto col_id : cols)
                    {
                        writeStringId(col_id);
                    }

                    // Write constraint name (optional)
                    writeStringId(unique_constraint->name());
                    continue;
                }

                // Handle PrimaryKeyConstraint
                auto *pk_constraint = dynamic_cast<parser::PrimaryKeyConstraint *>(constraint);
                if (pk_constraint)
                {
                    // Generate PRIMARY_KEY opcode for table-level PK
                    // Format: PRIMARY_KEY | col_count | col_names... | constraint_name
                    current_result_->writeOpcode(Opcode::PRIMARY_KEY);

                    // Write column count and names
                    const auto &cols = pk_constraint->columns();
                    if (cols.size() > UINT8_MAX)
                    {
                        current_result_->addError("Too many columns in PRIMARY KEY constraint (max 255)");
                        continue;
                    }
                    current_result_->writeByte(static_cast<uint8_t>(cols.size()));
                    for (auto col_id : cols)
                    {
                        writeStringId(col_id);
                    }

                    // Write constraint name (optional)
                    writeStringId(pk_constraint->name());
                }
            }
        }

        void BytecodeGenerator::visit(parser::CreateIndexStmt *node)
        {
            // Generate CREATE INDEX bytecode (Phase 2 Task 2.3 + Task 17)
            current_result_->writeOpcode(Opcode::CREATE_INDEX);

            // Write index name
            writeStringId(node->indexName());

            // Write table name
            writeStringId(node->tableName());

            // Write is_unique flag (1 byte: 0 = non-unique, 1 = unique)
            current_result_->writeByte(node->isUnique() ? 1 : 0);

            // Task 17: Separate simple columns from expressions
            const auto &index_columns = node->indexColumns();
            std::vector<parser::StringPool::StringId> simple_columns;
            std::vector<parser::Expression *> expressions;

            for (const auto &ic : index_columns)
            {
                if (ic.is_expression)
                {
                    expressions.push_back(ic.expression);
                }
                else
                {
                    simple_columns.push_back(ic.column_name);
                }
            }

            // Write simple column count and names
            current_result_->writeInt32(static_cast<uint32_t>(simple_columns.size()));
            for (auto column_id : simple_columns)
            {
                writeStringId(column_id);
            }

            // Write tablespace name (Phase 2 Task 2.3)
            writeStringId(node->tablespace());

            // LSM Integration Phase 2 Task 2.2: Write index type
            // Convert string index type to enum and serialize
            uint8_t index_type_byte = 0xFF;  // 0xFF = default (BTREE)
            if (node->hasIndexType())
            {
                std::string index_type_str = std::string(string_pool_.get(node->indexType()));
                auto index_type_opt = core::parseIndexType(index_type_str);
                if (index_type_opt.has_value())
                {
                    index_type_byte = static_cast<uint8_t>(index_type_opt.value());
                }
                // If parsing fails, we'll use 0xFF (default)
            }
            current_result_->writeByte(index_type_byte);

            // Task 17: Write expression/predicate flags
            bool has_expressions = !expressions.empty();
            bool has_predicate = node->hasWhereClause();

            current_result_->writeByte(has_expressions ? 1 : 0);
            current_result_->writeByte(has_predicate ? 1 : 0);

            // Serialize expressions
            if (has_expressions)
            {
                auto expr_data = core::ExpressionSerializer::serializeList(expressions);
                current_result_->writeInt32(static_cast<uint32_t>(expr_data.size()));
                for (uint8_t byte : expr_data)
                {
                    current_result_->writeByte(byte);
                }

                // Write original expression strings
                current_result_->writeInt32(static_cast<uint32_t>(expressions.size()));
                for (size_t i = 0; i < expressions.size(); i++)
                {
                    // For now, use generic placeholder
                    // TODO: Implement Expression::toString() for proper display
                    std::string expr_str = "<expression_" + std::to_string(i) + ">";
                    current_result_->writeString(expr_str);
                }
            }

            // Serialize predicate
            if (has_predicate)
            {
                parser::Expression *predicate = node->whereClause();
                auto pred_data = core::ExpressionSerializer::serialize(predicate);
                current_result_->writeInt32(static_cast<uint32_t>(pred_data.size()));
                for (uint8_t byte : pred_data)
                {
                    current_result_->writeByte(byte);
                }

                // Write original predicate string
                std::string pred_str = "<predicate>";
                current_result_->writeString(pred_str);
            }
        }

        void BytecodeGenerator::visit(parser::CreateTablespaceStmt *node)
        {
            // Generate CREATE TABLESPACE bytecode (Phase 2 Task 2.1)
            current_result_->writeOpcode(Opcode::CREATE_TABLESPACE);

            // Write tablespace name
            writeStringId(node->tablespaceName());

            // Write location path
            writeStringId(node->location());

            // Write autoextend_enabled (1 byte: 0 = OFF, 1 = ON)
            current_result_->writeByte(node->autoextendEnabled() ? 1 : 0);

            // Write autoextend_size_mb (uint32)
            current_result_->writeInt32(node->autoextendSizeMB());

            // Write max_size_mb (uint32, 0 = UNLIMITED)
            current_result_->writeInt32(node->maxSizeMB());

            // Write prealloc_pages (uint32)
            current_result_->writeInt32(node->preallocPages());
        }

        void BytecodeGenerator::visit(parser::AlterTablespaceStmt *node)
        {
            // Generate ALTER TABLESPACE bytecode (Phase 2 Task 2.2)
            current_result_->writeOpcode(Opcode::ALTER_TABLESPACE);

            // Write tablespace name
            writeStringId(node->tablespaceName());

            // Write number of alterations
            const auto &alterations = node->alterations();
            if (alterations.size() > UINT32_MAX)
            {
                current_result_->addError("Alteration count exceeds maximum");
                return;
            }
            current_result_->writeInt32(static_cast<uint32_t>(alterations.size()));

            // Write each alteration
            for (const auto &alt : alterations)
            {
                // Write alteration type (1 byte)
                current_result_->writeByte(static_cast<uint8_t>(alt.type));

                switch (alt.type)
                {
                    case parser::TablespaceAlterationType::SET_AUTOEXTEND:
                        // Write autoextend_enabled (1 byte)
                        current_result_->writeByte(alt.autoextend_enabled ? 1 : 0);
                        break;

                    case parser::TablespaceAlterationType::SET_AUTOEXTEND_SIZE:
                    case parser::TablespaceAlterationType::SET_MAXSIZE:
                        // Write size_value (uint32)
                        current_result_->writeInt32(alt.size_value);
                        break;

                    case parser::TablespaceAlterationType::RENAME_TO:
                        // Write new name (string)
                        writeStringId(alt.new_name);
                        break;
                }
            }
        }

        void BytecodeGenerator::visit(parser::DropTableStmt *node)
        {
            // Generate DROP TABLE bytecode (ALPHA Phase 1 - DDL Modifications)
            current_result_->writeOpcode(Opcode::DROP_TABLE);

            // Write table name
            writeStringId(node->tableName());

            // Write flags (1 byte: bit 0 = IF EXISTS, bit 1 = CASCADE)
            uint8_t flags = 0;
            if (node->ifExists())
            {
                flags |= 0x01;
            }
            if (node->dropBehavior() == parser::DropTableStmt::DropBehavior::CASCADE)
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::DropIndexStmt *node)
        {
            // Generate DROP INDEX bytecode (ALPHA Phase 1 - DDL Modifications)
            current_result_->writeOpcode(Opcode::DROP_INDEX);

            // Write index name
            writeStringId(node->indexName());

            // Write IF EXISTS flag (1 byte: 0 = no, 1 = yes)
            current_result_->writeByte(node->ifExists() ? 1 : 0);
        }

        void BytecodeGenerator::visit(parser::TruncateTableStmt *node)
        {
            // Generate TRUNCATE TABLE bytecode (ALPHA Phase 1 - DDL Modifications)
            current_result_->writeOpcode(Opcode::TRUNCATE_TABLE);

            // Write table name
            writeStringId(node->tableName());

            // Write mode (0=ASYNC, 1=SYNC)
            current_result_->writeByte(static_cast<uint8_t>(node->mode()));
        }

        void BytecodeGenerator::visit(parser::CreateSequenceStmt *node)
        {
            // Generate CREATE SEQUENCE bytecode (ALPHA Phase 1 - Sequences)
            current_result_->writeOpcode(Opcode::CREATE_SEQUENCE);

            // Write sequence name
            writeStringId(node->name());

            // Write optional parameters (use sentinel value 0x00 for "not specified")
            // Each parameter: 1 byte flag (0=not set, 1=set) + value if set

            // INCREMENT BY
            if (node->incrementBy())
            {
                current_result_->writeByte(0x01);
                node->incrementBy()->accept(this);  // Generate expression bytecode
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // MINVALUE / NO MINVALUE
            if (node->noMinValue())
            {
                current_result_->writeByte(0x02);  // NO MINVALUE flag
            }
            else if (node->minValue())
            {
                current_result_->writeByte(0x01);
                node->minValue()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // MAXVALUE / NO MAXVALUE
            if (node->noMaxValue())
            {
                current_result_->writeByte(0x02);  // NO MAXVALUE flag
            }
            else if (node->maxValue())
            {
                current_result_->writeByte(0x01);
                node->maxValue()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // START WITH
            if (node->startWith())
            {
                current_result_->writeByte(0x01);
                node->startWith()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // CACHE
            if (node->cache())
            {
                current_result_->writeByte(0x01);
                node->cache()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // CYCLE / NO CYCLE
            current_result_->writeByte(node->cycle() ? 0x01 : 0x00);
        }

        void BytecodeGenerator::visit(parser::AlterSequenceStmt *node)
        {
            // Generate ALTER SEQUENCE bytecode (ALPHA Phase 1 - Sequences)
            current_result_->writeOpcode(Opcode::ALTER_SEQUENCE);

            // Write sequence name
            writeStringId(node->name());

            // Write optional parameters (same format as CREATE SEQUENCE)

            // INCREMENT BY
            if (node->incrementBy())
            {
                current_result_->writeByte(0x01);
                node->incrementBy()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // MINVALUE / NO MINVALUE
            if (node->noMinValue())
            {
                current_result_->writeByte(0x02);
            }
            else if (node->minValue())
            {
                current_result_->writeByte(0x01);
                node->minValue()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // MAXVALUE / NO MAXVALUE
            if (node->noMaxValue())
            {
                current_result_->writeByte(0x02);
            }
            else if (node->maxValue())
            {
                current_result_->writeByte(0x01);
                node->maxValue()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // RESTART [WITH value]
            if (node->restart())
            {
                current_result_->writeByte(0x01);
                node->restart()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // CACHE
            if (node->cache())
            {
                current_result_->writeByte(0x01);
                node->cache()->accept(this);
            }
            else
            {
                current_result_->writeByte(0x00);
            }

            // CYCLE / NO CYCLE (3-state: 0=not set, 1=CYCLE, 2=NO CYCLE)
            if (node->hasCycle())
            {
                current_result_->writeByte(node->cycle() ? 0x01 : 0x02);
            }
            else
            {
                current_result_->writeByte(0x00);
            }
        }

        void BytecodeGenerator::visit(parser::DropSequenceStmt *node)
        {
            // Generate DROP SEQUENCE bytecode (ALPHA Phase 1 - Sequences)
            current_result_->writeOpcode(Opcode::DROP_SEQUENCE);

            // Write sequence name
            writeStringId(node->name());

            // Write flags (IF EXISTS, CASCADE)
            uint8_t flags = 0;
            if (node->ifExists())
                flags |= 0x01;
            if (node->cascade())
                flags |= 0x02;
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::CreateViewStmt *node)
        {
            // Generate CREATE VIEW bytecode (ALPHA Phase 1 - Views)
            current_result_->writeOpcode(Opcode::CREATE_VIEW);

            // Write view name
            writeStringId(node->name());

            // Write flags: [or_replace, check_option, has_column_names, materialized]
            uint8_t flags = 0;
            if (node->orReplace())
                flags |= 0x01;
            if (node->checkOption())
                flags |= 0x02;
            if (!node->columnNames().empty())
                flags |= 0x04;
            if (node->materialized())
                flags |= 0x08;  // Materialized view flag
            current_result_->writeByte(flags);

            // Write column names if present
            if (!node->columnNames().empty())
            {
                current_result_->writeByte(static_cast<uint8_t>(node->columnNames().size()));
                for (auto col_name : node->columnNames())
                {
                    writeStringId(col_name);
                }
            }

            // ALPHA Phase 1 - Views: Write actual SELECT query definition
            // The parser has extracted and stored the query text from the source
            current_result_->writeString(node->queryDefinitionText());
        }

        void BytecodeGenerator::visit(parser::DropViewStmt *node)
        {
            // Generate DROP VIEW bytecode (ALPHA Phase 1 - Views)
            current_result_->writeOpcode(Opcode::DROP_VIEW);

            // Write view name
            writeStringId(node->name());

            // Write flags (IF EXISTS, CASCADE)
            uint8_t flags = 0;
            if (node->ifExists())
                flags |= 0x01;
            if (node->cascade())
                flags |= 0x02;
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::RefreshMaterializedViewStmt *node)
        {
            // Generate REFRESH MATERIALIZED VIEW bytecode (ALPHA Phase 1 - Materialized Views)
            current_result_->writeOpcode(Opcode::REFRESH_MATERIALIZED_VIEW);

            // Write view name
            writeStringId(node->name());

            // Write CONCURRENTLY flag
            uint8_t flags = 0;
            if (node->concurrently())
                flags |= 0x01;
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::AlterTableStmt *node)
        {
            // Generate ALTER TABLE bytecode (ALPHA Phase 1 - DDL Modifications)
            current_result_->writeOpcode(Opcode::ALTER_TABLE);

            // Write table name
            writeStringId(node->tableName());

            // Write action type (1 byte)
            current_result_->writeByte(static_cast<uint8_t>(node->action()));

            // Write action-specific parameters
            switch (node->action())
            {
                case parser::AlterTableStmt::AlterAction::ADD_COLUMN:
                {
                    auto *col_def = node->columnDef();
                    if (col_def)
                    {
                        // Write column name
                        writeStringId(col_def->name());

                        // Write data type
                        const auto &type = col_def->type();
                        current_result_->writeByte(static_cast<uint8_t>(type.type));

                        // Write precision
                        current_result_->writeInt32(type.precision);

                        // Write scale
                        current_result_->writeInt32(type.scale);

                        // Write nullable flag
                        current_result_->writeByte(col_def->nullable() ? 1 : 0);
                    }
                    break;
                }

                case parser::AlterTableStmt::AlterAction::DROP_COLUMN:
                {
                    // Write column name
                    writeStringId(node->dropColumnName());

                    // Write IF EXISTS flag
                    current_result_->writeByte(node->ifExists() ? 1 : 0);

                    // Write CASCADE flag
                    current_result_->writeByte(node->dropBehavior() == parser::AlterTableStmt::DropBehavior::CASCADE ? 1 : 0);
                    break;
                }

                case parser::AlterTableStmt::AlterAction::RENAME_COLUMN:
                {
                    // Write old column name
                    writeStringId(node->oldColumnName());

                    // Write new column name
                    writeStringId(node->newColumnName());
                    break;
                }

                case parser::AlterTableStmt::AlterAction::ALTER_COLUMN_TYPE:
                {
                    // Write column name
                    writeStringId(node->oldColumnName());

                    // Write new data type
                    auto *new_type = node->newType();
                    if (new_type)
                    {
                        current_result_->writeByte(static_cast<uint8_t>(new_type->type));
                        current_result_->writeInt32(new_type->precision);
                        current_result_->writeInt32(new_type->scale);
                    }
                    break;
                }

                default:
                    // Other actions not implemented yet
                    break;
            }
        }

        void BytecodeGenerator::visit(parser::DropTablespaceStmt *node)
        {
            // Generate DROP TABLESPACE bytecode (Phase 2 Task 2.1)
            current_result_->writeOpcode(Opcode::DROP_TABLESPACE);

            // Write tablespace name
            writeStringId(node->tablespaceName());

            // Write force flag (1 byte: 0 = normal, 1 = FORCE)
            current_result_->writeByte(node->force() ? 1 : 0);
        }

        void BytecodeGenerator::visit(parser::AttachTablespaceStmt *node)
        {
            // Generate ATTACH TABLESPACE bytecode (Phase 6 Task 6.1)
            current_result_->writeOpcode(Opcode::ATTACH_TABLESPACE);

            // Write file path
            writeStringId(node->filePath());

            // Write optional tablespace name
            writeStringId(node->tablespaceName());
        }

        void BytecodeGenerator::visit(parser::DetachTablespaceStmt *node)
        {
            // Generate DETACH TABLESPACE bytecode (Phase 6 Task 6.2)
            current_result_->writeOpcode(Opcode::DETACH_TABLESPACE);

            // Write tablespace name
            writeStringId(node->tablespaceName());

            // Write force flag (1 byte: 0 = normal, 1 = FORCE)
            current_result_->writeByte(node->force() ? 1 : 0);
        }

        void BytecodeGenerator::visit(parser::AlterTableSetTablespaceStmt *node)
        {
            // Generate ALTER TABLE SET TABLESPACE bytecode (Phase 4 Task 4.1.6)
            current_result_->writeOpcode(Opcode::ALTER_TABLE_SET_TABLESPACE);

            // Write table name
            writeStringId(node->tableName());

            // Write tablespace name
            writeStringId(node->tablespaceName());

            // Write online flag (1 byte: 0 = offline, 1 = online)
            current_result_->writeByte(node->online() ? 1 : 0);
        }

        void BytecodeGenerator::visit(parser::InsertStmt *node)
        {
            current_result_->writeOpcode(Opcode::INSERT);

            // Write table name
            current_result_->writeOpcode(Opcode::TABLE_REF);
            writeStringId(node->tableName());

            // Write column list with overflow check
            size_t col_count = node->columns().size();
            if (col_count > UINT32_MAX)
            {
                current_result_->addError("Column count exceeds maximum (4 billion)");
                return;
            }

            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(col_count));

            for (auto col_id : node->columns())
            {
                current_result_->writeOpcode(Opcode::COLUMN_REF);
                writeStringId(col_id);
            }

            current_result_->writeOpcode(Opcode::END_LIST);

            // Write value list with overflow check
            size_t val_count = node->values().size();
            if (val_count > UINT32_MAX)
            {
                current_result_->addError("Value count exceeds maximum (4 billion)");
                return;
            }

            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(val_count));

            for (auto *value : node->values())
            {
                // Null check handled in generateExpression
                generateExpression(value);
            }

            current_result_->writeOpcode(Opcode::END_LIST);

            // Emit RETURNING clause if present (Alpha 1 - Advanced SQL)
            if (node->hasReturning())
            {
                current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_RETURNING));

                // Write column count
                current_result_->writeInt32(static_cast<uint32_t>(node->returningColumns().size()));

                // Write column names
                for (const auto& col_id : node->returningColumns())
                {
                    writeStringId(col_id);
                }
            }
        }

        void BytecodeGenerator::visit(parser::SelectStmt *node)
        {
            // Phase 2 Wave 2: Handle WITH clause (CTEs) if present
            if (node->withClause())
            {
                const auto& ctes = node->withClause()->ctes();

                // Emit WITH_CLAUSE marker with CTE count and recursive flag
                current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_WITH_CLAUSE));
                // Write count as 2 bytes (uint16_t)
                uint16_t cte_count = static_cast<uint16_t>(ctes.size());
                current_result_->writeByte(static_cast<uint8_t>(cte_count & 0xFF));
                current_result_->writeByte(static_cast<uint8_t>((cte_count >> 8) & 0xFF));
                // Write recursive flag (1 byte)
                current_result_->writeByte(node->withClause()->isRecursive() ? 1 : 0);

                // Process each CTE
                for (const auto& cte : ctes)
                {
                    // Add CTE name to active set
                    active_ctes_.insert(cte.name);

                    // Emit CTE_DEF marker
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CTE_DEF));

                    // Write CTE name
                    writeStringId(cte.name);

                    // Generate bytecode for CTE query (recursively)
                    cte.query->accept(this);
                }
            }

            // Try query planner if available (Phase 1, Task 1.3)
            if (database_ && database_->query_planner())
            {
                core::ErrorContext ctx;
                auto plan = database_->query_planner()->planQuery(node, string_pool_, &ctx);

                if (plan)
                {
                    // Use optimized plan
                    DEBUG_LOG_DB("Using optimized query plan");
                    generateFromPlan(plan, node);

                    // Clear active CTEs after query completes
                    if (node->withClause())
                    {
                        active_ctes_.clear();
                    }
                    return;
                }

                // Planning failed - log warning and fallback
                if (!ctx.message.empty())
                {
                    DEBUG_LOG_DB("Query planning failed: " + ctx.message + ", falling back to direct generation");
                }
            }

            // Fallback: Direct bytecode generation (no optimization)
            DEBUG_LOG_DB("Using direct SELECT generation (no optimization)");
            generateDirectSelect(node);

            // Clear active CTEs after query completes
            if (node->withClause())
            {
                active_ctes_.clear();
            }
        }

        void BytecodeGenerator::generateDirectSelect(parser::SelectStmt *node)
        {
            current_result_->writeOpcode(Opcode::SELECT);

            // Write select list with overflow check
            size_t select_count = node->selectList().size();
            if (select_count > UINT32_MAX)
            {
                current_result_->addError("SELECT list count exceeds maximum (4 billion)");
                return;
            }

            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(select_count));

            for (const auto &item : node->selectList())
            {
                if (item.is_star)
                {
                    current_result_->writeOpcode(Opcode::SELECT_STAR);
                }
                else
                {
                    generateExpression(item.expr);

                    // Handle aliases - write alias string ID if present
                    if (item.alias != 0)
                    {
                        current_result_->writeOpcode(Opcode::COLUMN_REF);
                        writeStringId(item.alias);
                    }
                }
            }

            current_result_->writeOpcode(Opcode::END_LIST);

            // Write table name or CTE reference (Phase 2 Wave 2)
            if (isActiveCTE(node->tableName()))
            {
                // This is a CTE reference
                current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CTE_SCAN));
                writeStringId(node->tableName());
            }
            else
            {
                // Regular table reference
                current_result_->writeOpcode(Opcode::TABLE_REF);
                writeStringId(node->tableName());
            }

            // Write WHERE clause if present
            if (node->whereClause())
            {
                current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
                // Null check handled in generateExpression
                generateExpression(node->whereClause());
            }
        }

        void BytecodeGenerator::visit(parser::SetOperationStmt *node)
        {
            // Emit appropriate set operation opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);

            switch (node->opType())
            {
                case parser::SetOperationType::UNION:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_UNION));
                    break;
                case parser::SetOperationType::UNION_ALL:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_UNION_ALL));
                    break;
                case parser::SetOperationType::INTERSECT:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_INTERSECT));
                    break;
                case parser::SetOperationType::INTERSECT_ALL:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_INTERSECT_ALL));
                    break;
                case parser::SetOperationType::EXCEPT:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_EXCEPT));
                    break;
                case parser::SetOperationType::EXCEPT_ALL:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_EXCEPT_ALL));
                    break;
            }

            // Generate bytecode for left side
            node->left()->accept(this);

            // Generate bytecode for right side
            node->right()->accept(this);

            // Handle ORDER BY (applies to final result)
            if (!node->orderByClause().empty())
            {
                current_result_->writeOpcode(Opcode::ORDER_BY);
                size_t order_count = node->orderByClause().size();
                if (order_count > UINT32_MAX)
                {
                    current_result_->addError("ORDER BY clause has too many items");
                    return;
                }
                current_result_->writeInt32(static_cast<uint32_t>(order_count));

                for (const auto &item : node->orderByClause())
                {
                    generateExpression(item.expr);

                    // Write sort direction
                    if (item.order == parser::SortOrder::ASC)
                    {
                        current_result_->writeOpcode(Opcode::SORT_ASC);
                    }
                    else
                    {
                        current_result_->writeOpcode(Opcode::SORT_DESC);
                    }

                    // Write nulls ordering
                    if (item.nulls_order == parser::NullsOrder::NULLS_FIRST)
                    {
                        current_result_->writeOpcode(Opcode::NULLS_FIRST);
                    }
                    else if (item.nulls_order == parser::NullsOrder::NULLS_LAST)
                    {
                        current_result_->writeOpcode(Opcode::NULLS_LAST);
                    }
                }
            }

            // Handle LIMIT/OFFSET (applies to final result)
            if (node->hasLimit())
            {
                current_result_->writeOpcode(Opcode::LIMIT);
                current_result_->writeInt64(static_cast<uint64_t>(node->limitCount()));
            }

            if (node->hasOffset())
            {
                current_result_->writeOpcode(Opcode::OFFSET);
                current_result_->writeInt64(static_cast<uint64_t>(node->offsetCount()));
            }
        }

        void BytecodeGenerator::visit(parser::UpdateStmt *node)
        {
            // Phase 1 Task 2.1: UPDATE statement bytecode generation
            current_result_->writeOpcode(Opcode::UPDATE);

            // Write table name
            current_result_->writeOpcode(Opcode::TABLE_REF);
            writeStringId(node->tableName());

            // Write assignments list
            size_t assignment_count = node->assignments().size();
            if (assignment_count > UINT32_MAX)
            {
                current_result_->addError("Assignment count exceeds maximum (4 billion)");
                return;
            }

            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(assignment_count));

            for (const auto &assignment : node->assignments())
            {
                current_result_->writeOpcode(Opcode::ASSIGNMENT);
                // Write column name
                current_result_->writeOpcode(Opcode::COLUMN_REF);
                writeStringId(assignment.column_name);
                // Write value expression
                generateExpression(assignment.value);
            }

            current_result_->writeOpcode(Opcode::END_LIST);

            // Write WHERE clause if present
            if (node->whereClause())
            {
                current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
                generateExpression(node->whereClause());
            }

            // Emit RETURNING clause if present (Alpha 1 - Advanced SQL)
            if (node->hasReturning())
            {
                current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_RETURNING));

                // Write column count
                current_result_->writeInt32(static_cast<uint32_t>(node->returningColumns().size()));

                // Write column names
                for (const auto& col_id : node->returningColumns())
                {
                    writeStringId(col_id);
                }
            }
        }

        void BytecodeGenerator::visit(parser::DeleteStmt *node)
        {
            // Phase 1 Task 2.2: DELETE statement bytecode generation
            current_result_->writeOpcode(Opcode::DELETE);

            // Write table name
            current_result_->writeOpcode(Opcode::TABLE_REF);
            writeStringId(node->tableName());

            // Write WHERE clause if present
            if (node->whereClause())
            {
                current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
                generateExpression(node->whereClause());
            }

            // Emit RETURNING clause if present (Alpha 1 - Advanced SQL)
            if (node->hasReturning())
            {
                current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_RETURNING));

                // Write column count
                current_result_->writeInt32(static_cast<uint32_t>(node->returningColumns().size()));

                // Write column names
                for (const auto& col_id : node->returningColumns())
                {
                    writeStringId(col_id);
                }
            }
        }

        void BytecodeGenerator::visit(parser::MergeStmt *node)
        {
            // Alpha 1 - Advanced SQL: MERGE statement bytecode generation
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_START));

            // Write target table name
            writeStringId(node->targetTable());

            // Write source (MERGE_SOURCE opcode)
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_SOURCE));
            generateExpression(node->source());

            // Write ON condition (MERGE_ON opcode)
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_ON));
            generateExpression(node->onCondition());

            // Write WHEN clauses
            for (const auto& when_clause : node->whenClauses())
            {
                switch (when_clause.type)
                {
                    case parser::MergeStmt::WhenClause::MATCHED:
                    {
                        // WHEN MATCHED THEN UPDATE
                        current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                        current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_WHEN_MATCHED));

                        // Write number of assignments
                        current_result_->writeInt32(static_cast<uint32_t>(when_clause.assignments.size()));

                        // Write each assignment (column_name + value_expr)
                        for (const auto& assignment : when_clause.assignments)
                        {
                            writeStringId(assignment.column_name);
                            generateExpression(assignment.value);
                        }
                        break;
                    }

                    case parser::MergeStmt::WhenClause::NOT_MATCHED:
                    {
                        // WHEN NOT MATCHED THEN INSERT
                        current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                        current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_WHEN_NOT_MATCHED));

                        // Write column count
                        current_result_->writeInt32(static_cast<uint32_t>(when_clause.insert_columns.size()));

                        // Write column names
                        for (const auto& column_id : when_clause.insert_columns)
                        {
                            writeStringId(column_id);
                        }

                        // Write value expressions
                        for (const auto& value_expr : when_clause.insert_values)
                        {
                            generateExpression(value_expr);
                        }
                        break;
                    }

                    case parser::MergeStmt::WhenClause::NOT_MATCHED_BY_SOURCE:
                    {
                        // WHEN NOT MATCHED BY SOURCE THEN DELETE
                        current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                        current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_WHEN_NOT_MATCHED_SOURCE));
                        break;
                    }
                }
            }

            // Write MERGE_END marker
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_END));
        }

        void BytecodeGenerator::visit(parser::StartTransactionStmt *node)
        {
            // Generate START TRANSACTION bytecode (Phase 2 Task 2.6, Phase 3 Task 3.6)
            current_result_->writeOpcode(Opcode::START_TRANSACTION);

            // Write transaction mode (1 byte: 0 = READ_WRITE, 1 = READ_ONLY)
            current_result_->writeByte(node->mode() == parser::TransactionMode::READ_ONLY ? 1 : 0);

            // Write isolation level (1 byte: 0 = READ_COMMITTED, 1 = SNAPSHOT, 2 =
            // SNAPSHOT_TABLE_STABILITY)
            uint8_t isolation_byte = 0;
            switch (node->isolation())
            {
                case parser::IsolationLevel::READ_COMMITTED:
                    isolation_byte = 0;
                    break;
                case parser::IsolationLevel::SNAPSHOT:
                    isolation_byte = 1;
                    break;
                case parser::IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                    isolation_byte = 2;
                    break;
            }
            current_result_->writeByte(isolation_byte);

            // Write wait flag (1 byte: 0 = NO WAIT, 1 = WAIT)
            current_result_->writeByte(node->wait() ? 1 : 0);

            // Write commit outstanding flag (1 byte: 0 = false, 1 = true) - START_TRANSACTION only
            current_result_->writeByte(node->commitOutstanding() ? 1 : 0);

            // Write lock timeout (uint32, 0 = no lock timeout)
            current_result_->writeInt32(node->lockTimeout());

            // Write table reservations list (Phase 3 Task 3.6)
            const auto &reservations = node->tableReservations();
            if (reservations.size() > UINT32_MAX)
            {
                current_result_->addError("Table reservation count exceeds maximum");
                return;
            }

            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(reservations.size()));

            for (const auto &res : reservations)
            {
                // Write table name
                current_result_->writeOpcode(Opcode::TABLE_REF);
                writeStringId(res.table_name);

                // Write lock mode (1 byte: 0 = SHARED, 1 = PROTECTED)
                current_result_->writeByte(res.lock_mode == parser::TableLockMode::SHARED ? 0 : 1);

                // Write for_write flag (1 byte: 0 = FOR READ, 1 = FOR WRITE)
                current_result_->writeByte(res.for_write ? 1 : 0);
            }

            current_result_->writeOpcode(Opcode::END_LIST);
        }

        void BytecodeGenerator::visit(parser::SetTransactionStmt *node)
        {
            // Generate SET TRANSACTION bytecode (Phase 3 Task 3.6)
            current_result_->writeOpcode(Opcode::SET_TRANSACTION);

            // Write transaction mode (1 byte: 0 = READ_WRITE, 1 = READ_ONLY)
            current_result_->writeByte(node->mode() == parser::TransactionMode::READ_ONLY ? 1 : 0);

            // Write isolation level (1 byte: 0 = READ_COMMITTED, 1 = SNAPSHOT, 2 =
            // SNAPSHOT_TABLE_STABILITY)
            uint8_t isolation_byte = 0;
            switch (node->isolation())
            {
                case parser::IsolationLevel::READ_COMMITTED:
                    isolation_byte = 0;
                    break;
                case parser::IsolationLevel::SNAPSHOT:
                    isolation_byte = 1;
                    break;
                case parser::IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                    isolation_byte = 2;
                    break;
            }
            current_result_->writeByte(isolation_byte);

            // Write wait flag (1 byte: 0 = NO WAIT, 1 = WAIT)
            current_result_->writeByte(node->wait() ? 1 : 0);

            // Write lock timeout (uint32, 0 = no lock timeout)
            current_result_->writeInt32(node->lockTimeout());

            // Write table reservations list (Phase 3 Task 3.6)
            const auto &reservations = node->tableReservations();
            if (reservations.size() > UINT32_MAX)
            {
                current_result_->addError("Table reservation count exceeds maximum");
                return;
            }

            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(reservations.size()));

            for (const auto &res : reservations)
            {
                // Write table name
                current_result_->writeOpcode(Opcode::TABLE_REF);
                writeStringId(res.table_name);

                // Write lock mode (1 byte: 0 = SHARED, 1 = PROTECTED)
                current_result_->writeByte(res.lock_mode == parser::TableLockMode::SHARED ? 0 : 1);

                // Write for_write flag (1 byte: 0 = FOR READ, 1 = FOR WRITE)
                current_result_->writeByte(res.for_write ? 1 : 0);
            }

            current_result_->writeOpcode(Opcode::END_LIST);
        }

        void BytecodeGenerator::visit(parser::CommitStmt *node)
        {
            // Generate COMMIT bytecode (Phase 2 Task 2.6)
            current_result_->writeOpcode(Opcode::COMMIT);
            (void)node; // Suppress unused parameter warning
        }

        void BytecodeGenerator::visit(parser::RollbackStmt *node)
        {
            // Generate ROLLBACK bytecode (Phase 2 Task 2.6)
            current_result_->writeOpcode(Opcode::ROLLBACK);
            (void)node; // Suppress unused parameter warning
        }

        void BytecodeGenerator::visit(parser::SweepStmt *node)
        {
            // Generate SWEEP DATABASE bytecode (Phase 3 Task 3.3)
            current_result_->writeOpcode(Opcode::SWEEP);
            (void)node; // Suppress unused parameter warning
        }

        void BytecodeGenerator::visit(parser::ShowStmt *node)
        {
            // SHOW commands - bytecode generation (ALPHA Phase 1 - Developer Experience)
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);

            // Write appropriate extended opcode based on object type
            switch (node->objectType())
            {
                case parser::ShowObjectType::TABLES:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHOW_TABLES));
                    // Write optional database name
                    writeStringId(node->databaseName());
                    // Write optional LIKE pattern
                    writeStringId(node->likePattern());
                    break;

                case parser::ShowObjectType::DATABASES:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHOW_DATABASES));
                    // Write optional LIKE pattern
                    writeStringId(node->likePattern());
                    break;

                case parser::ShowObjectType::COLUMNS:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHOW_COLUMNS));
                    // Write table name (required)
                    writeStringId(node->tableName());
                    // Write optional LIKE pattern
                    writeStringId(node->likePattern());
                    break;

                case parser::ShowObjectType::INDEXES:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHOW_INDEXES));
                    // Write table name (required)
                    writeStringId(node->tableName());
                    break;

                case parser::ShowObjectType::CREATE_TABLE:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHOW_CREATE_TABLE));
                    // Write table name (required)
                    writeStringId(node->tableName());
                    break;
            }

            DEBUG_LOG_DB("Generated SHOW bytecode");
        }

        void BytecodeGenerator::visit(parser::DescribeStmt *node)
        {
            // DESCRIBE command - bytecode generation (ALPHA Phase 1 - Developer Experience)
            // DESCRIBE is an alias for SHOW COLUMNS
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DESCRIBE_TABLE));

            // Write table name (required)
            writeStringId(node->tableName());

            DEBUG_LOG_DB("Generated DESCRIBE bytecode");
        }

        void BytecodeGenerator::visit(parser::AnalyzeStmt *node)
        {
            // Generate ANALYZE bytecode (Phase 1 Task 1.1.2)
            // NOTE: ANALYZE is not yet implemented in bytecode executor
            // For now, just add an error
            current_result_->addError("ANALYZE statement bytecode generation not yet implemented");
            (void)node; // Suppress unused parameter warning
        }

        void BytecodeGenerator::visit(parser::ExplainStmt *node)
        {
            // Generate EXPLAIN bytecode (Phase 1 Task 1.5)
            // EXPLAIN shows the query plan without executing the query

            // Check if database available for planning
            if (!database_ || !database_->query_planner())
            {
                current_result_->addError("EXPLAIN requires database with query planner");
                return;
            }

            // Only SELECT supported in Phase 1.5
            auto *select_stmt = dynamic_cast<parser::SelectStmt*>(node->query());
            if (!select_stmt)
            {
                current_result_->addError("EXPLAIN only supports SELECT statements");
                return;
            }

            // Generate query plan
            core::ErrorContext ctx;
            auto plan = database_->query_planner()->planQuery(select_stmt, string_pool_, &ctx);

            if (!plan)
            {
                std::string error = "Query planning failed";
                if (!ctx.message.empty())
                {
                    error += ": " + ctx.message;
                }
                current_result_->addError(error);
                return;
            }

            // Format plan as EXPLAIN text using PlanNode::toString()
            std::string explain_output = "                        QUERY PLAN\n";
            explain_output += "--------------------------------------------------------\n";
            explain_output += plan->toString(0);

            // Write EXPLAIN output to bytecode
            current_result_->writeOpcode(Opcode::EXPLAIN_PLAN);
            current_result_->writeString(explain_output);

            DEBUG_LOG_DB("Generated EXPLAIN output for query");
        }

        // ===== Expression Visitors =====

        void BytecodeGenerator::visit(parser::LiteralExpr *node)
        {
            switch (node->literalType())
            {
                case parser::LiteralExpr::INTEGER:
                    current_result_->writeOpcode(Opcode::LITERAL_INT64);
                    current_result_->writeInt64(static_cast<uint64_t>(node->intValue()));
                    break;

                case parser::LiteralExpr::FLOAT:
                    current_result_->writeOpcode(Opcode::LITERAL_DOUBLE);
                    current_result_->writeDouble(node->floatValue());
                    break;

                case parser::LiteralExpr::STRING:
                    current_result_->writeOpcode(Opcode::LITERAL_STRING);
                    writeStringId(node->stringValue());
                    break;

                case parser::LiteralExpr::NULL_LITERAL:
                    current_result_->writeOpcode(Opcode::LITERAL_NULL);
                    break;
            }
        }

        void BytecodeGenerator::visit(parser::IdentifierExpr *node)
        {
            // Phase 1 Task 3.1: Handle qualified column references
            current_result_->writeOpcode(Opcode::COLUMN_REF);

            // Write qualifier if present (for table.column references)
            if (node->isQualified())
            {
                writeStringId(node->qualifier());
            }
            else
            {
                // Write 0 to indicate no qualifier
                writeStringId(0);
            }

            // Write column name
            writeStringId(node->name());
        }

        void BytecodeGenerator::visit(parser::BinaryOpExpr *node)
        {
            // Generate left operand
            generateExpression(node->left());

            // Generate right operand
            generateExpression(node->right());

            // Generate operation
            switch (node->op())
            {
                case parser::BinaryOp::ADD:
                    current_result_->writeOpcode(Opcode::EXPR_ADD);
                    break;
                case parser::BinaryOp::SUBTRACT:
                    current_result_->writeOpcode(Opcode::EXPR_SUBTRACT);
                    break;
                case parser::BinaryOp::MULTIPLY:
                    current_result_->writeOpcode(Opcode::EXPR_MULTIPLY);
                    break;
                case parser::BinaryOp::DIVIDE:
                    current_result_->writeOpcode(Opcode::EXPR_DIVIDE);
                    break;
                case parser::BinaryOp::MODULO:
                    current_result_->writeOpcode(Opcode::EXPR_MODULO);
                    break;
                case parser::BinaryOp::EQ:
                    current_result_->writeOpcode(Opcode::EXPR_EQ);
                    break;
                case parser::BinaryOp::NE:
                    current_result_->writeOpcode(Opcode::EXPR_NE);
                    break;
                case parser::BinaryOp::LT:
                    current_result_->writeOpcode(Opcode::EXPR_LT);
                    break;
                case parser::BinaryOp::GT:
                    current_result_->writeOpcode(Opcode::EXPR_GT);
                    break;
                case parser::BinaryOp::LE:
                    current_result_->writeOpcode(Opcode::EXPR_LE);
                    break;
                case parser::BinaryOp::GE:
                    current_result_->writeOpcode(Opcode::EXPR_GE);
                    break;
                case parser::BinaryOp::AND:
                    current_result_->writeOpcode(Opcode::EXPR_AND);
                    break;
                case parser::BinaryOp::OR:
                    current_result_->writeOpcode(Opcode::EXPR_OR);
                    break;
                case parser::BinaryOp::LIKE:
                    current_result_->writeOpcode(Opcode::EXPR_LIKE);
                    break;
                case parser::BinaryOp::ILIKE:
                    current_result_->writeOpcode(Opcode::EXPR_ILIKE);
                    break;
                // Array operators (Phase 2 Task 12)
                case parser::BinaryOp::ARRAY_OVERLAP:
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_OVERLAP));
                    break;
                case parser::BinaryOp::ARRAY_CONTAINS:
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_CONTAINS));
                    break;
                case parser::BinaryOp::ARRAY_CONTAINED_BY:
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_CONTAINED_BY));
                    break;
                // Regex operators (Phase 2 Task 13)
                case parser::BinaryOp::REGEX_MATCH:
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEX_MATCH));
                    break;
                case parser::BinaryOp::REGEX_MATCH_CI:
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEX_MATCH_CI));
                    break;
                case parser::BinaryOp::REGEX_NOT_MATCH:
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEX_NOT_MATCH));
                    break;
                case parser::BinaryOp::REGEX_NOT_MATCH_CI:
                    current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEX_NOT_MATCH_CI));
                    break;
            }
        }

        void BytecodeGenerator::visit(parser::CastExpr *node)
        {
            // Generate the expression being cast
            generateExpression(node->expr());

            // Write CAST opcode
            current_result_->writeOpcode(Opcode::EXPR_CAST);

            // Write try_cast flag (1 byte: 0 = CAST, 1 = TRY_CAST)
            current_result_->writeByte(node->isTryCast() ? 1 : 0);

            // Write target type
            writeDataType(node->targetType());
        }

        void BytecodeGenerator::visit(parser::FunctionCallExpr *node)
        {
            // Get function name
            std::string_view func_name = string_pool_.get(node->name());

            // Map function names to opcodes
            Opcode func_opcode;
            if (func_name == "LENGTH")
            {
                func_opcode = Opcode::FUNC_LENGTH;
            }
            else if (func_name == "SUBSTRING")
            {
                func_opcode = Opcode::FUNC_SUBSTRING;
            }
            else if (func_name == "UPPER")
            {
                func_opcode = Opcode::FUNC_UPPER;
            }
            else if (func_name == "LOWER")
            {
                func_opcode = Opcode::FUNC_LOWER;
            }
            else if (func_name == "TRIM")
            {
                func_opcode = Opcode::FUNC_TRIM;
            }
            else if (func_name == "SUM")
            {
                func_opcode = Opcode::AGG_SUM;
            }
            else if (func_name == "AVG")
            {
                func_opcode = Opcode::AGG_AVG;
            }
            else if (func_name == "MIN")
            {
                func_opcode = Opcode::AGG_MIN;
            }
            else if (func_name == "MAX")
            {
                func_opcode = Opcode::AGG_MAX;
            }
            else if (func_name == "COUNT")
            {
                func_opcode = Opcode::AGG_COUNT;
            }
            else if (func_name == "DATE_ADD")
            {
                func_opcode = Opcode::FUNC_DATE_ADD;
            }
            else if (func_name == "DATE_SUB")
            {
                func_opcode = Opcode::FUNC_DATE_SUB;
            }
            else if (func_name == "DATE_DIFF" || func_name == "DATEDIFF")
            {
                func_opcode = Opcode::FUNC_DATE_DIFF;
            }
            else if (func_name == "NOW")
            {
                func_opcode = Opcode::FUNC_NOW;
            }
            else if (func_name == "CURRENT_DATE")
            {
                func_opcode = Opcode::FUNC_CURRENT_DATE;
            }
            // Spatial functions (Phase 2 Task 9.1)
            else if (func_name == "ST_POINT")
            {
                // ST_Point(x, y) - create point from coordinates
                // Generate arguments first
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                // Emit extended opcode
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_POINT));
                return;
            }
            else if (func_name == "ST_MAKELINE")
            {
                // ST_MakeLine(point1, point2, ...) - create linestring from points
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_MAKELINE));
                // Write argument count for variable args
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ST_MAKEPOLYGON")
            {
                // ST_MakePolygon(linestring) - create polygon from linestring
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_MAKEPOLYGON));
                return;
            }
            else if (func_name == "ST_ASTEXT")
            {
                // ST_AsText(geom) - convert geometry to WKT string
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_ASTEXT));
                return;
            }
            else if (func_name == "ST_ASBINARY")
            {
                // ST_AsBinary(geom) - convert geometry to WKB binary
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_ASBINARY));
                return;
            }
            else if (func_name == "ST_GEOMETRYTYPE")
            {
                // ST_GeometryType(geom) - get geometry type name
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_GEOMETRYTYPE));
                return;
            }
            else if (func_name == "ST_ISVALID")
            {
                // ST_IsValid(geom) - validate geometry
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_ISVALID));
                return;
            }
            // Spatial geometric operations (Phase 2 Task 9.3)
            else if (func_name == "ST_BUFFER")
            {
                // ST_Buffer(geom, distance) - create buffer polygon around geometry
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Buffer expects 2 arguments (geometry, distance)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_BUFFER));
                return;
            }
            else if (func_name == "ST_CONVEXHULL")
            {
                // ST_ConvexHull(geom) - compute convex hull of geometry
                if (node->args().size() != 1)
                {
                    throw std::runtime_error("ST_ConvexHull expects 1 argument (geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_CONVEXHULL));
                return;
            }
            else if (func_name == "ST_ENVELOPE")
            {
                // ST_Envelope(geom) - compute minimum bounding box (envelope) of geometry
                if (node->args().size() != 1)
                {
                    throw std::runtime_error("ST_Envelope expects 1 argument (geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_ENVELOPE));
                return;
            }
            // Spatial predicates (Phase 2 Task 9.3 - G2/G4)
            else if (func_name == "ST_INTERSECTS")
            {
                // ST_Intersects(geom1, geom2) - do geometries intersect?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Intersects expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_INTERSECTS));
                return;
            }
            else if (func_name == "ST_CONTAINS")
            {
                // ST_Contains(geom1, geom2) - does geom1 contain geom2?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Contains expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_CONTAINS));
                return;
            }
            else if (func_name == "ST_WITHIN")
            {
                // ST_Within(geom1, geom2) - is geom1 within geom2?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Within expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_WITHIN));
                return;
            }
            else if (func_name == "ST_EQUALS")
            {
                // ST_Equals(geom1, geom2) - are geometries spatially equal?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Equals expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_EQUALS));
                return;
            }
            else if (func_name == "ST_DISJOINT")
            {
                // ST_Disjoint(geom1, geom2) - are geometries disjoint?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Disjoint expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_DISJOINT));
                return;
            }
            else if (func_name == "ST_OVERLAPS")
            {
                // ST_Overlaps(geom1, geom2) - do geometries overlap?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Overlaps expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_OVERLAPS));
                return;
            }
            else if (func_name == "ST_TOUCHES")
            {
                // ST_Touches(geom1, geom2) - do geometries touch?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Touches expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_TOUCHES));
                return;
            }
            else if (func_name == "ST_CROSSES")
            {
                // ST_Crosses(geom1, geom2) - do geometries cross?
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Crosses expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_CROSSES));
                return;
            }
            // Spatial processing functions (Phase 2 Task 9.3 - G4)
            else if (func_name == "ST_INTERSECTION")
            {
                // ST_Intersection(geom1, geom2) - compute intersection geometry
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Intersection expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_INTERSECTION));
                return;
            }
            else if (func_name == "ST_UNION")
            {
                // ST_Union(geom1, geom2) - compute union geometry
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Union expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_UNION));
                return;
            }
            else if (func_name == "ST_DIFFERENCE")
            {
                // ST_Difference(geom1, geom2) - compute difference (geom1 - geom2)
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Difference expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_DIFFERENCE));
                return;
            }
            // Spatial metrics (Phase 2 Task 9.3 - G4)
            else if (func_name == "ST_AREA")
            {
                // ST_Area(geom) - compute area of polygon
                if (node->args().size() != 1)
                {
                    throw std::runtime_error("ST_Area expects 1 argument (geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_AREA));
                return;
            }
            else if (func_name == "ST_LENGTH")
            {
                // ST_Length(geom) - compute length of linestring
                if (node->args().size() != 1)
                {
                    throw std::runtime_error("ST_Length expects 1 argument (geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_LENGTH));
                return;
            }
            else if (func_name == "ST_DISTANCE")
            {
                // ST_Distance(geom1, geom2) - compute distance between geometries
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Distance expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_DISTANCE));
                return;
            }
            else if (func_name == "ST_PERIMETER")
            {
                // ST_Perimeter(geom) - compute perimeter of polygon
                if (node->args().size() != 1)
                {
                    throw std::runtime_error("ST_Perimeter expects 1 argument (geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_PERIMETER));
                return;
            }
            // SRID functions (Phase 2 Task 9.5)
            else if (func_name == "ST_SRID")
            {
                // ST_SRID(geom) - get SRID of geometry
                if (node->args().size() != 1)
                {
                    throw std::runtime_error("ST_SRID expects 1 argument (geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_SRID));
                return;
            }
            else if (func_name == "ST_SETSRID")
            {
                // ST_SetSRID(geom, srid) - set SRID of geometry
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_SetSRID expects 2 arguments (geometry, integer)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_SETSRID));
                return;
            }
            else if (func_name == "ST_TRANSFORM")
            {
                // ST_Transform(geom, target_srid) - transform to different SRID
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Transform expects 2 arguments (geometry, integer)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_TRANSFORM));
                return;
            }
            else if (func_name == "ST_DISTANCE_SPHERE")
            {
                // ST_Distance_Sphere(geom1, geom2) - geodetic distance
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_Distance_Sphere expects 2 arguments (geometry, geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_DISTANCE_SPHERE));
                return;
            }
            // Multi-geometry constructor functions (OGC Simple Features)
            else if (func_name == "ST_MULTIPOINT")
            {
                // ST_MultiPoint(point1, point2, ...) - create MULTIPOINT from points
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_MULTIPOINT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ST_MULTILINESTRING")
            {
                // ST_MultiLineString(linestring1, linestring2, ...) - create MULTILINESTRING
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_MULTILINESTRING));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ST_MULTIPOLYGON")
            {
                // ST_MultiPolygon(polygon1, polygon2, ...) - create MULTIPOLYGON
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_MULTIPOLYGON));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ST_GEOMETRYCOLLECTION")
            {
                // ST_GeometryCollection(geom1, geom2, ...) - create heterogeneous collection
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_GEOMETRYCOLLECTION));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ST_COLLECT")
            {
                // ST_Collect(geom1, geom2, ...) - alias for ST_GeometryCollection
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_COLLECT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Multi-geometry accessor functions
            else if (func_name == "ST_NUMGEOMETRIES")
            {
                // ST_NumGeometries(multi_geom) - get count of geometries in collection
                if (node->args().size() != 1)
                {
                    throw std::runtime_error("ST_NumGeometries expects 1 argument (multi-geometry)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_NUMGEOMETRIES));
                return;
            }
            else if (func_name == "ST_GEOMETRYN")
            {
                // ST_GeometryN(multi_geom, n) - get Nth geometry (1-indexed)
                if (node->args().size() != 2)
                {
                    throw std::runtime_error("ST_GeometryN expects 2 arguments (multi-geometry, integer)");
                }
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ST_GEOMETRYN));
                return;
            }
            // Array functions (Phase 2 Task 12)
            else if (func_name == "ARRAY_TO_STRING" || func_name == "KW_ARRAY_TO_STRING")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::ARRAY_TO_STRING));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "STRING_TO_ARRAY" || func_name == "KW_STRING_TO_ARRAY")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::STRING_TO_ARRAY));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ARRAY_APPEND" || func_name == "KW_ARRAY_APPEND")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_APPEND));
                return;
            }
            else if (func_name == "ARRAY_PREPEND" || func_name == "KW_ARRAY_PREPEND")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_PREPEND));
                return;
            }
            else if (func_name == "ARRAY_CAT" || func_name == "KW_ARRAY_CAT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_CAT));
                return;
            }
            else if (func_name == "ARRAY_REMOVE" || func_name == "KW_ARRAY_REMOVE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_REMOVE));
                return;
            }
            else if (func_name == "ARRAY_REPLACE" || func_name == "KW_ARRAY_REPLACE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_REPLACE));
                return;
            }
            else if (func_name == "ARRAY_LENGTH" || func_name == "KW_ARRAY_LENGTH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_LENGTH));
                return;
            }
            else if (func_name == "ARRAY_DIMS" || func_name == "KW_ARRAY_DIMS")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_DIMS));
                return;
            }
            else if (func_name == "ARRAY_UPPER" || func_name == "KW_ARRAY_UPPER")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_UPPER));
                return;
            }
            else if (func_name == "ARRAY_LOWER" || func_name == "KW_ARRAY_LOWER")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_LOWER));
                return;
            }
            else if (func_name == "UNNEST" || func_name == "KW_UNNEST")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeOpcode(Opcode::UNNEST);
                return;
            }
            // Text search and regex functions (Phase 2 Task 13)
            else if (func_name == "REGEXP_MATCHES")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEXP_MATCHES));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "REGEXP_REPLACE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEXP_REPLACE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "REGEXP_SPLIT_TO_ARRAY")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEXP_SPLIT_TO_ARRAY));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "REGEXP_SPLIT_TO_TABLE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REGEXP_SPLIT_TO_TABLE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SPLIT_PART")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SPLIT_PART));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "STRING_TO_TABLE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_STRING_TO_TABLE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "UNNEST_TEXT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_UNNEST_TEXT));
                return;
            }
            else if (func_name == "STRPOS")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_STRPOS));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "POSITION")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_POSITION));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "OVERLAY")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_OVERLAY));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "QUOTE_LITERAL")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_QUOTE_LITERAL));
                return;
            }
            else if (func_name == "QUOTE_IDENT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_QUOTE_IDENT));
                return;
            }
            else if (func_name == "INITCAP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_INITCAP));
                return;
            }
            else if (func_name == "ASCII")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ASCII));
                return;
            }
            else if (func_name == "CHR")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CHR));
                return;
            }
            else if (func_name == "REPEAT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REPEAT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "REVERSE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REVERSE));
                return;
            }
            else if (func_name == "LPAD")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_LPAD));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "RPAD")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_RPAD));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "OVERLAY")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_OVERLAY));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Mathematical Functions (ALPHA Phase A - Critical Priority)
            // Trigonometric functions
            else if (func_name == "SIN")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_SIN));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "COS")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_COS));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "TAN")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_TAN));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ASIN")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ASIN));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ACOS")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ACOS));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ATAN")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ATAN));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ATAN2")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ATAN2));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Angle conversion
            else if (func_name == "DEGREES")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_DEGREES));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "RADIANS")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_RADIANS));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "PI")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_PI));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Algebraic functions
            else if (func_name == "ABS")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ABS));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SIGN")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_SIGN));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ROUND")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ROUND));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "CEIL" || func_name == "CEILING")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_CEIL));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "FLOOR")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_FLOOR));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "TRUNC" || func_name == "TRUNCATE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_TRUNC));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "MOD")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_MOD));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SQRT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_SQRT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "CBRT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_CBRT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Hyperbolic functions
            else if (func_name == "SINH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_SINH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "COSH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_COSH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "TANH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_TANH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ASINH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ASINH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ACOSH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ACOSH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ATANH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_ATANH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "COT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_COT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "POWER" || func_name == "POW")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_POWER));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "EXP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_EXP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Logarithmic functions
            else if (func_name == "LN")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_LN));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "LOG")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_LOG));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "LOG10")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_LOG10));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "LOG2")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNC_LOG2));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Statistical Functions (Nov 14, 2025)
            else if (func_name == "STDDEV" || func_name == "STDDEV_SAMP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_STDDEV_SAMP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "STDDEV_POP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_STDDEV_POP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "VARIANCE" || func_name == "VAR_SAMP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_VAR_SAMP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "VAR_POP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_VAR_POP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "CORR")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_CORR));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "COVAR_POP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_COVAR_POP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Cryptographic Functions (Nov 14, 2025)
            else if (func_name == "MD5")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MD5));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SHA1")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHA1));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SHA256")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHA256));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SHA512")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHA512));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "ENCODE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ENCODE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "DECODE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DECODE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // XML Functions (Nov 14, 2025)
            else if (func_name == "XMLPARSE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLPARSE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLSERIALIZE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLSERIALIZE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLELEMENT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLELEMENT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLCONCAT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLCONCAT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLFOREST")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLFOREST));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLCOMMENT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLCOMMENT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLROOT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLROOT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XPATH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XPATH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLEXISTS")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLEXISTS));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Bit Manipulation Functions (Nov 14, 2025)
            else if (func_name == "GET_BYTE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_GET_BYTE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SET_BYTE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SET_BYTE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "GET_BIT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_GET_BIT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SET_BIT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SET_BIT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_AND")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_AND));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_OR")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_OR));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_XOR")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_XOR));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_NOT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_NOT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_SHIFT_LEFT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_SHIFT_LEFT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_SHIFT_RIGHT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_SHIFT_RIGHT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_SHIFT_RIGHT_LOGICAL")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_SHIFT_RIGHT_LOGICAL));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_COUNT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_COUNT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_LENGTH")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_LENGTH));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "BIT_MASK")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BIT_MASK));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Statistical Functions (Nov 14, 2025)
            else if (func_name == "STDDEV" || func_name == "STDDEV_SAMP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_STDDEV_SAMP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "STDDEV_POP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_STDDEV_POP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "VARIANCE" || func_name == "VAR_SAMP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_VAR_SAMP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "VAR_POP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_VAR_POP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "CORR")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_CORR));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "COVAR_POP")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::AGG_COVAR_POP));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // Cryptographic Functions (Nov 14, 2025)
            else if (func_name == "MD5")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MD5));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SHA1")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHA1));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SHA256")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHA256));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "SHA512")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SHA512));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            // XML Functions (Nov 14, 2025)
            else if (func_name == "XMLPARSE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLPARSE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLSERIALIZE")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLSERIALIZE));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLELEMENT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLELEMENT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLCONCAT")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLCONCAT));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else if (func_name == "XMLFOREST")
            {
                for (auto *arg : node->args())
                {
                    generateExpression(arg);
                }
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_XMLFOREST));
                current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
                return;
            }
            else
            {
                current_result_->addError("Unknown function: " + std::string(func_name));
                return;
            }

            // Generate arguments first (reverse Polish notation)
            for (auto *arg : node->args())
            {
                generateExpression(arg);
            }

            // Write function opcode
            current_result_->writeOpcode(func_opcode);

            // Write argument count for validation
            if (node->args().size() > UINT8_MAX)
            {
                current_result_->addError("Function has too many arguments (max 255)");
                return;
            }
            current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
        }

        void BytecodeGenerator::visit(parser::ColumnDef *node)
        {
            current_result_->writeOpcode(Opcode::COLUMN_DEF);

            // Write column name
            current_result_->writeOpcode(Opcode::COLUMN_REF);
            writeStringId(node->name());

            // Write data type
            writeDataType(node->type());

            // Write constraints
            if (!node->nullable())
            {
                current_result_->writeOpcode(Opcode::NOT_NULL);
            }

            // Write DEFAULT expression if present (ALPHA Phase A - Constraint Enforcement)
            if (node->default_value() != nullptr)
            {
                current_result_->writeOpcode(Opcode::DEFAULT_VALUE);

                // Generate bytecode for DEFAULT expression
                BytecodeResult temp_result;
                BytecodeResult *saved_result = current_result_;
                current_result_ = &temp_result;

                // Generate expression bytecode
                node->default_value()->accept(this);

                // Restore result buffer
                current_result_ = saved_result;

                // Write the generated bytecode length and data
                const auto &bytecode = temp_result.bytecode();
                if (bytecode.size() > UINT32_MAX)
                {
                    current_result_->addError("DEFAULT expression bytecode too large");
                    return;
                }
                current_result_->writeInt32(static_cast<uint32_t>(bytecode.size()));
                for (uint8_t byte : bytecode)
                {
                    current_result_->writeByte(byte);
                }
            }

            // Write CHECK expression if present (ALPHA Phase A - Constraint Enforcement)
            if (node->check_expr() != nullptr)
            {
                current_result_->writeOpcode(Opcode::CHECK_CONSTRAINT);

                // Generate bytecode for CHECK expression
                BytecodeResult temp_result;
                BytecodeResult *saved_result = current_result_;
                current_result_ = &temp_result;

                // Generate expression bytecode
                node->check_expr()->accept(this);

                // Restore result buffer
                current_result_ = saved_result;

                // Write the generated bytecode length and data
                const auto &bytecode = temp_result.bytecode();
                if (bytecode.size() > UINT32_MAX)
                {
                    current_result_->addError("CHECK expression bytecode too large");
                    return;
                }
                current_result_->writeInt32(static_cast<uint32_t>(bytecode.size()));
                for (uint8_t byte : bytecode)
                {
                    current_result_->writeByte(byte);
                }
            }

            // Write UNIQUE constraint if present
            if (node->isUnique())
            {
                current_result_->writeOpcode(Opcode::UNIQUE_CONSTRAINT);
            }

            // Write PRIMARY KEY constraint if present
            if (node->isPrimaryKey())
            {
                current_result_->writeOpcode(Opcode::PRIMARY_KEY);
            }

            // Write FOREIGN KEY constraint if present (ALPHA Phase A - FK Constraints)
            if (node->fk_table() != 0)
            {
                current_result_->writeOpcode(Opcode::FOREIGN_KEY);

                // Write referenced table name
                writeStringId(node->fk_table());

                // Write foreign key column count and names
                const auto &fk_cols = node->fk_columns();
                if (fk_cols.size() > UINT8_MAX)
                {
                    current_result_->addError("Too many foreign key columns (max 255)");
                    return;
                }
                current_result_->writeByte(static_cast<uint8_t>(fk_cols.size()));
                for (auto col_id : fk_cols)
                {
                    writeStringId(col_id);
                }

                // Write ON DELETE action
                writeStringId(node->fk_on_delete());

                // Write ON UPDATE action
                writeStringId(node->fk_on_update());
            }

            // Write IDENTITY column constraint if present (ALPHA Phase 1 - IDENTITY Columns Phase 3)
            if (node->isIdentity())
            {
                current_result_->writeOpcode(Opcode::IDENTITY_COLUMN);

                // Write identity type: 1 byte (1 = ALWAYS, 0 = BY DEFAULT)
                current_result_->writeByte(node->identityAlways() ? 1 : 0);
            }

            // Write GENERATED column constraint if present (ALPHA Phase 1 - Constraint Features)
            if (node->isGenerated() && node->generationExpr() != nullptr)
            {
                current_result_->writeOpcode(Opcode::GENERATED_COLUMN);

                // Write storage type: 1 byte (1 = STORED, 2 = VIRTUAL)
                uint8_t storage_type = static_cast<uint8_t>(node->generatedStorage());
                current_result_->writeByte(storage_type);

                // Generate bytecode for generation expression
                BytecodeResult temp_result;
                BytecodeResult *saved_result = current_result_;
                current_result_ = &temp_result;

                // Generate expression bytecode
                node->generationExpr()->accept(this);

                // Restore result buffer
                current_result_ = saved_result;

                // Write the generated bytecode length and data
                const auto &bytecode = temp_result.bytecode();
                if (bytecode.size() > UINT32_MAX)
                {
                    current_result_->addError("GENERATED column expression bytecode too large");
                    return;
                }
                current_result_->writeInt32(static_cast<uint32_t>(bytecode.size()));
                for (uint8_t byte : bytecode)
                {
                    current_result_->writeByte(byte);
                }
            }
        }

        // ===== Security Statements (ALPHA Phase 1 - Security System Phase 2) =====

        void BytecodeGenerator::visit(parser::CreateUserStmt *node)
        {
            // Emit extended opcode marker + CREATE_USER opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CREATE_USER));

            // Write username (StringPool ID)
            writeStringId(node->username());

            // Write flags byte: bit 0 = has_password, bit 1 = is_superuser
            uint8_t flags = 0;
            if (node->hasPassword())
            {
                flags |= 0x01;
            }
            if (node->isSuperuser())
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);

            // Write password if present (StringPool ID)
            if (node->hasPassword())
            {
                writeStringId(node->password());
            }
        }

        void BytecodeGenerator::visit(parser::AlterUserStmt *node)
        {
            // Emit extended opcode marker + ALTER_USER opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ALTER_USER));

            // Write username (StringPool ID)
            writeStringId(node->username());

            // Write flags byte: bit 0 = change_password, bit 1 = change_superuser, bit 2 = is_superuser
            uint8_t flags = 0;
            if (node->changePassword())
            {
                flags |= 0x01;
            }
            if (node->changeSuperuser())
            {
                flags |= 0x02;
            }
            if (node->isSuperuser())
            {
                flags |= 0x04;
            }
            current_result_->writeByte(flags);

            // Write password if changing (StringPool ID)
            if (node->changePassword())
            {
                writeStringId(node->password());
            }
        }

        void BytecodeGenerator::visit(parser::DropUserStmt *node)
        {
            // Emit extended opcode marker + DROP_USER opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DROP_USER));

            // Write username (StringPool ID)
            writeStringId(node->username());

            // Write flags byte: bit 0 = if_exists, bit 1 = cascade (vs restrict)
            uint8_t flags = 0;
            if (node->ifExists())
            {
                flags |= 0x01;
            }
            if (node->dropBehavior() == parser::DropUserStmt::DropBehavior::CASCADE)
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::CreateRoleStmt *node)
        {
            // Emit extended opcode marker + CREATE_ROLE opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CREATE_ROLE));

            // Write rolename (StringPool ID)
            writeStringId(node->rolename());
        }

        void BytecodeGenerator::visit(parser::DropRoleStmt *node)
        {
            // Emit extended opcode marker + DROP_ROLE opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DROP_ROLE));

            // Write rolename (StringPool ID)
            writeStringId(node->rolename());

            // Write flags byte: bit 0 = if_exists, bit 1 = cascade (vs restrict)
            uint8_t flags = 0;
            if (node->ifExists())
            {
                flags |= 0x01;
            }
            if (node->dropBehavior() == parser::DropRoleStmt::DropBehavior::CASCADE)
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::CreateGroupStmt *node)
        {
            // Emit extended opcode marker + CREATE_GROUP opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CREATE_GROUP));

            // Write groupname (StringPool ID)
            writeStringId(node->groupname());
        }

        void BytecodeGenerator::visit(parser::DropGroupStmt *node)
        {
            // Emit extended opcode marker + DROP_GROUP opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DROP_GROUP));

            // Write groupname (StringPool ID)
            writeStringId(node->groupname());

            // Write flags byte: bit 0 = if_exists, bit 1 = cascade (vs restrict)
            uint8_t flags = 0;
            if (node->ifExists())
            {
                flags |= 0x01;
            }
            if (node->dropBehavior() == parser::DropGroupStmt::DropBehavior::CASCADE)
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::GrantPrivilegeStmt *node)
        {
            // Emit extended opcode marker + GRANT_PRIVILEGE opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_GRANT_PRIVILEGE));

            // Write privilege bitmask (uint32_t)
            current_result_->writeInt32(node->privileges());

            // Write object type (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->objectType()));

            // Write object name (StringPool ID)
            writeStringId(node->objectName());

            // Write grantee type (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->granteeType()));

            // Write grantee name (StringPool ID, 0 if PUBLIC)
            writeStringId(node->granteeName());

            // Write flags byte: bit 0 = with_grant_option, bit 1 = has_column_list
            uint8_t flags = 0;
            if (node->withGrantOption())
            {
                flags |= 0x01;
            }
            if (node->hasColumnList())  // Security Phase 3.3.4
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);

            // Security Phase 3.3.4: Write column list if present
            if (node->hasColumnList())
            {
                // Write column count (uint32_t)
                current_result_->writeInt32(static_cast<uint32_t>(node->columnNames().size()));

                // Write each column name (StringPool ID)
                for (auto col_id : node->columnNames())
                {
                    writeStringId(col_id);
                }
            }
        }

        void BytecodeGenerator::visit(parser::RevokePrivilegeStmt *node)
        {
            // Emit extended opcode marker + REVOKE_PRIVILEGE opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REVOKE_PRIVILEGE));

            // Write privilege bitmask (uint32_t)
            current_result_->writeInt32(node->privileges());

            // Write object type (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->objectType()));

            // Write object name (StringPool ID)
            writeStringId(node->objectName());

            // Write grantee type (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->granteeType()));

            // Write grantee name (StringPool ID, 0 if PUBLIC)
            writeStringId(node->granteeName());

            // Write flags byte: bit 0 = cascade (vs restrict), bit 1 = has_column_list
            uint8_t flags = 0;
            if (node->revokeBehavior() == parser::RevokePrivilegeStmt::RevokeBehavior::CASCADE)
            {
                flags |= 0x01;
            }
            if (node->hasColumnList())  // Security Phase 3.3.4
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);

            // Security Phase 3.3.4: Write column list if present
            if (node->hasColumnList())
            {
                // Write column count (uint32_t)
                current_result_->writeInt32(static_cast<uint32_t>(node->columnNames().size()));

                // Write each column name (StringPool ID)
                for (auto col_id : node->columnNames())
                {
                    writeStringId(col_id);
                }
            }
        }

        void BytecodeGenerator::visit(parser::GrantRoleStmt *node)
        {
            // Emit extended opcode marker + GRANT_ROLE opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_GRANT_ROLE));

            // Write role name (StringPool ID)
            writeStringId(node->rolename());

            // Write grantee type (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->granteeType()));

            // Write grantee name (StringPool ID)
            writeStringId(node->granteeName());
        }

        void BytecodeGenerator::visit(parser::RevokeRoleStmt *node)
        {
            // Emit extended opcode marker + REVOKE_ROLE opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_REVOKE_ROLE));

            // Write role name (StringPool ID)
            writeStringId(node->rolename());

            // Write grantee type (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->granteeType()));

            // Write grantee name (StringPool ID)
            writeStringId(node->granteeName());

            // Write flags byte: bit 0 = cascade (vs restrict)
            uint8_t flags = 0;
            if (node->revokeBehavior() == parser::RevokeRoleStmt::RevokeBehavior::CASCADE)
            {
                flags |= 0x01;
            }
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::SetRoleStmt *node)
        {
            // Emit extended opcode marker + SET_ROLE opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SET_ROLE));

            // Write flags byte: bit 0 = is_reset (RESET ROLE vs SET ROLE)
            uint8_t flags = 0;
            if (node->isReset())
            {
                flags |= 0x01;
            }
            current_result_->writeByte(flags);

            // Write role name if not reset (StringPool ID, 0 if reset)
            if (!node->isReset())
            {
                writeStringId(node->rolename());
            }
        }

        void BytecodeGenerator::visit(parser::SetSessionAuthStmt *node)
        {
            // Emit extended opcode marker + SET_SESSION_AUTH opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SET_SESSION_AUTH));

            // Write flags byte: bit 0 = is_reset (RESET SESSION AUTHORIZATION vs SET)
            uint8_t flags = 0;
            if (node->isReset())
            {
                flags |= 0x01;
            }
            current_result_->writeByte(flags);

            // Write username if not reset (StringPool ID, 0 if reset)
            if (!node->isReset())
            {
                writeStringId(node->username());
            }
        }

        // Security Phase 3.4.4 - Row-Level Security Policy Statements
        void BytecodeGenerator::visit(parser::CreatePolicyStmt *node)
        {
            // Emit extended opcode marker + CREATE_POLICY opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CREATE_POLICY));

            // Write policy name (StringPool ID)
            writeStringId(node->policyName());

            // Write table name (StringPool ID)
            writeStringId(node->tableName());

            // Write policy command (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->command()));

            // Write role count and roles
            current_result_->writeInt32(static_cast<uint32_t>(node->roles().size()));
            for (auto role_id : node->roles())
            {
                writeStringId(role_id);
            }

            // Write flags byte: bit 0 = has_using_expr, bit 1 = has_with_check_expr
            uint8_t flags = 0;
            if (node->hasUsingExpr())
            {
                flags |= 0x01;
            }
            if (node->hasWithCheckExpr())
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);

            // Write USING expression if present
            if (node->hasUsingExpr())
            {
                generateExpression(node->usingExpr());
            }

            // Write WITH CHECK expression if present
            if (node->hasWithCheckExpr())
            {
                generateExpression(node->withCheckExpr());
            }
        }

        void BytecodeGenerator::visit(parser::DropPolicyStmt *node)
        {
            // Emit extended opcode marker + DROP_POLICY opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DROP_POLICY));

            // Write policy name (StringPool ID)
            writeStringId(node->policyName());

            // Write table name (StringPool ID)
            writeStringId(node->tableName());

            // Write flags byte: bit 0 = if_exists, bit 1 = cascade (vs restrict)
            uint8_t flags = 0;
            if (node->ifExists())
            {
                flags |= 0x01;
            }
            if (node->dropBehavior() == parser::DropPolicyStmt::DropBehavior::CASCADE)
            {
                flags |= 0x02;
            }
            current_result_->writeByte(flags);
        }

        void BytecodeGenerator::visit(parser::AlterTableRLSStmt *node)
        {
            // Emit extended opcode marker + ALTER_TABLE_RLS opcode
            current_result_->writeOpcode(Opcode::EXTENDED_OPCODE);
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ALTER_TABLE_RLS));

            // Write table name (StringPool ID)
            writeStringId(node->tableName());

            // Write RLS action (uint8_t enum value)
            current_result_->writeByte(static_cast<uint8_t>(node->action()));
        }

        // ===== Query Planner Integration (Phase 1, Task 1.3) =====

        BytecodeResult BytecodeGenerator::generateFromPlan(
            std::shared_ptr<optimizer::PlanNode> plan,
            parser::SelectStmt *original_stmt)
        {
            BytecodeResult result;
            current_result_ = &result;

            // Write version header
            result.writeOpcode(Opcode::VERSION);
            result.writeByte(SBLR_VERSION);

            // Dispatch based on plan node type
            switch (plan->type())
            {
            case scratchbird::optimizer::PlanNodeType::SEQ_SCAN:
                generateSeqScanPlan(
                    static_cast<scratchbird::optimizer::SeqScanNode *>(plan.get()),
                    original_stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::INDEX_SCAN:
                generateIndexScanPlan(
                    static_cast<scratchbird::optimizer::IndexScanNode *>(plan.get()),
                    original_stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::NESTED_LOOP_JOIN:
                generateNestedLoopJoinPlan(
                    static_cast<scratchbird::optimizer::NestedLoopJoinNode *>(plan.get()),
                    original_stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::HASH_JOIN:
                generateHashJoinPlan(
                    static_cast<scratchbird::optimizer::HashJoinNode *>(plan.get()),
                    original_stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::AGGREGATE:
                generateAggregatePlan(
                    static_cast<scratchbird::optimizer::AggregateNode *>(plan.get()),
                    original_stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::SORT:
                generateSortPlan(
                    static_cast<scratchbird::optimizer::SortNode *>(plan.get()),
                    original_stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::LIMIT:
                generateLimitPlan(
                    static_cast<scratchbird::optimizer::LimitNode *>(plan.get()),
                    original_stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::WINDOW:
                generateWindowPlan(
                    static_cast<scratchbird::optimizer::WindowNode *>(plan.get()),
                    original_stmt);
                break;

            default:
                result.addError("Unsupported plan node type");
                break;
            }

            // End marker
            result.writeOpcode(Opcode::END);

            current_result_ = nullptr;
            return result;
        }

        void BytecodeGenerator::generateSeqScanPlan(
            scratchbird::optimizer::SeqScanNode *node,
            parser::SelectStmt *stmt)
        {
            // Generate SELECT bytecode
            current_result_->writeOpcode(Opcode::SELECT);

            // Write select list (from original stmt)
            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(stmt->selectList().size()));

            for (const auto &item : stmt->selectList())
            {
                if (item.is_star)
                {
                    current_result_->writeOpcode(Opcode::SELECT_STAR);
                }
                else
                {
                    generateExpression(item.expr);
                    if (item.alias != 0)
                    {
                        current_result_->writeOpcode(Opcode::COLUMN_REF);
                        writeStringId(item.alias);
                    }
                }
            }

            current_result_->writeOpcode(Opcode::END_LIST);

            // Write table reference
            current_result_->writeOpcode(Opcode::TABLE_REF);
            writeStringId(stmt->tableName());

            // Write WHERE clause
            if (stmt->whereClause())
            {
                current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
                generateExpression(stmt->whereClause());
            }

            // Write scan hint (for future executor optimization)
            current_result_->writeOpcode(Opcode::SCAN_HINT);
            current_result_->writeByte(0); // 0 = Sequential scan

            DEBUG_LOG_DB("Generated SeqScan bytecode with optimizer hint");
        }

        void BytecodeGenerator::generateIndexScanPlan(
            scratchbird::optimizer::IndexScanNode *node,
            parser::SelectStmt *stmt)
        {
            // Generate SELECT bytecode
            current_result_->writeOpcode(Opcode::SELECT);

            // Write select list
            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(stmt->selectList().size()));

            for (const auto &item : stmt->selectList())
            {
                if (item.is_star)
                {
                    current_result_->writeOpcode(Opcode::SELECT_STAR);
                }
                else
                {
                    generateExpression(item.expr);
                    if (item.alias != 0)
                    {
                        current_result_->writeOpcode(Opcode::COLUMN_REF);
                        writeStringId(item.alias);
                    }
                }
            }

            current_result_->writeOpcode(Opcode::END_LIST);

            // Write table reference
            current_result_->writeOpcode(Opcode::TABLE_REF);
            writeStringId(stmt->tableName());

            // Write index reference (NEW: tells executor which index to use)
            current_result_->writeOpcode(Opcode::INDEX_REF);
            current_result_->writeString(node->indexId().toString());

            // Write WHERE clause
            if (stmt->whereClause())
            {
                current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
                generateExpression(stmt->whereClause());
            }

            // Write scan hint
            current_result_->writeOpcode(Opcode::SCAN_HINT);
            current_result_->writeByte(1); // 1 = Index scan

            DEBUG_LOG_DB("Generated IndexScan bytecode with optimizer hint (index: " +
                         node->indexName() + ")");
        }

        // ===== JOIN Plan Node Bytecode Generation (Phase 1, Task 3.3) =====

        void BytecodeGenerator::generateNestedLoopJoinPlan(
            scratchbird::optimizer::NestedLoopJoinNode *node,
            parser::SelectStmt *stmt)
        {
            DEBUG_LOG_DB("Generating Nested Loop Join bytecode");

            // Write JOIN opcode
            current_result_->writeOpcode(Opcode::NESTED_LOOP_JOIN);

            // Write join type (INNER=0, LEFT=1, RIGHT=2, FULL=3)
            current_result_->writeOpcode(Opcode::JOIN_TYPE);
            current_result_->writeByte(static_cast<uint8_t>(node->joinType()));

            // Generate bytecode for outer (left) child
            generateJoinPlan(node->outerPlan().get(), stmt);

            // Generate bytecode for inner (right) child
            generateJoinPlan(node->innerPlan().get(), stmt);

            // Write join condition
            if (node->joinCondition())
            {
                current_result_->writeOpcode(Opcode::JOIN_CONDITION);
                generateExpression(node->joinCondition());
            }

            DEBUG_LOG_DB("Generated Nested Loop Join bytecode");
        }

        void BytecodeGenerator::generateHashJoinPlan(
            scratchbird::optimizer::HashJoinNode *node,
            parser::SelectStmt *stmt)
        {
            DEBUG_LOG_DB("Generating Hash Join bytecode");

            // Write JOIN opcode
            current_result_->writeOpcode(Opcode::HASH_JOIN);

            // Write join type
            current_result_->writeOpcode(Opcode::JOIN_TYPE);
            current_result_->writeByte(static_cast<uint8_t>(node->joinType()));

            // Generate bytecode for outer (probe) child
            generateJoinPlan(node->outerPlan().get(), stmt);

            // Generate bytecode for inner (build) child
            generateJoinPlan(node->innerPlan().get(), stmt);

            // Write hash keys for outer side
            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(node->hashKeysOuter().size()));
            for (auto *key : node->hashKeysOuter())
            {
                generateExpression(key);
            }
            current_result_->writeOpcode(Opcode::END_LIST);

            // Write hash keys for inner side
            current_result_->writeOpcode(Opcode::BEGIN_LIST);
            current_result_->writeInt32(static_cast<uint32_t>(node->hashKeysInner().size()));
            for (auto *key : node->hashKeysInner())
            {
                generateExpression(key);
            }
            current_result_->writeOpcode(Opcode::END_LIST);

            // Write join condition (for non-equality predicates)
            if (node->joinCondition())
            {
                current_result_->writeOpcode(Opcode::JOIN_CONDITION);
                generateExpression(node->joinCondition());
            }

            DEBUG_LOG_DB("Generated Hash Join bytecode");
        }

        void BytecodeGenerator::generateJoinPlan(
            scratchbird::optimizer::PlanNode *node,
            parser::SelectStmt *stmt)
        {
            // Recursively generate bytecode for JOIN tree nodes
            switch (node->type())
            {
            case scratchbird::optimizer::PlanNodeType::SEQ_SCAN:
            {
                auto *scan_node = static_cast<scratchbird::optimizer::SeqScanNode *>(node);

                // Write SELECT for this table scan
                current_result_->writeOpcode(Opcode::SELECT);

                // Write select list (SELECT *)
                current_result_->writeOpcode(Opcode::BEGIN_LIST);
                current_result_->writeInt32(1);
                current_result_->writeOpcode(Opcode::SELECT_STAR);
                current_result_->writeOpcode(Opcode::END_LIST);

                // Write table reference
                current_result_->writeOpcode(Opcode::TABLE_REF);
                current_result_->writeString(scan_node->tableName());

                // Write scan hint
                current_result_->writeOpcode(Opcode::SCAN_HINT);
                current_result_->writeByte(0); // 0 = Sequential scan
                break;
            }

            case scratchbird::optimizer::PlanNodeType::INDEX_SCAN:
            {
                auto *idx_node = static_cast<scratchbird::optimizer::IndexScanNode *>(node);

                // Write SELECT for this table scan
                current_result_->writeOpcode(Opcode::SELECT);

                // Write select list (SELECT *)
                current_result_->writeOpcode(Opcode::BEGIN_LIST);
                current_result_->writeInt32(1);
                current_result_->writeOpcode(Opcode::SELECT_STAR);
                current_result_->writeOpcode(Opcode::END_LIST);

                // Write table reference
                current_result_->writeOpcode(Opcode::TABLE_REF);
                current_result_->writeString(idx_node->tableName());

                // Write index reference
                current_result_->writeOpcode(Opcode::INDEX_REF);
                current_result_->writeString(idx_node->indexId().toString());

                // Write scan hint
                current_result_->writeOpcode(Opcode::SCAN_HINT);
                current_result_->writeByte(1); // 1 = Index scan
                break;
            }

            case scratchbird::optimizer::PlanNodeType::NESTED_LOOP_JOIN:
                generateNestedLoopJoinPlan(
                    static_cast<scratchbird::optimizer::NestedLoopJoinNode *>(node),
                    stmt);
                break;

            case scratchbird::optimizer::PlanNodeType::HASH_JOIN:
                generateHashJoinPlan(
                    static_cast<scratchbird::optimizer::HashJoinNode *>(node),
                    stmt);
                break;

            default:
                current_result_->addError("Unsupported JOIN child node type");
                break;
            }
        }

        // ===== Aggregation, Sorting, and Limiting Implementation (Phase 1, Tasks 4-5) =====

        void BytecodeGenerator::visit(parser::AggregateExpr *node)
        {
            generateAggregateFunc(node);
        }

        void BytecodeGenerator::generateAggregateFunc(parser::AggregateExpr *agg_expr)
        {
            // Emit aggregate function opcode
            switch (agg_expr->func())
            {
            case parser::AggregateFunc::COUNT:
                current_result_->writeOpcode(Opcode::AGG_COUNT);
                break;
            case parser::AggregateFunc::SUM:
                current_result_->writeOpcode(Opcode::AGG_SUM);
                break;
            case parser::AggregateFunc::AVG:
                current_result_->writeOpcode(Opcode::AGG_AVG);
                break;
            case parser::AggregateFunc::MIN:
                current_result_->writeOpcode(Opcode::AGG_MIN);
                break;
            case parser::AggregateFunc::MAX:
                current_result_->writeOpcode(Opcode::AGG_MAX);
                break;
            case parser::AggregateFunc::ARRAY_AGG:  // Phase 2 Task 12
                current_result_->writeOpcode(Opcode::ARRAY_AGG);
                break;
            }

            // Write DISTINCT flag
            current_result_->writeByte(agg_expr->distinct() ? 1 : 0);

            // Write argument expression (nullptr for COUNT(*))
            if (agg_expr->arg())
            {
                generateExpression(agg_expr->arg());
            }
            else
            {
                // COUNT(*) - no argument
                current_result_->writeOpcode(Opcode::SELECT_STAR);
            }
        }

        void BytecodeGenerator::generateAggregatePlan(
            scratchbird::optimizer::AggregateNode *node,
            parser::SelectStmt *stmt)
        {
            // For aggregate plans, we need to recursively generate the child plan first
            // Then layer the aggregation on top

            // Write SELECT opcode
            current_result_->writeOpcode(Opcode::SELECT);

            // Generate child plan (the input to aggregation)
            auto child_plan = node->childPlan();
            generateJoinPlan(child_plan.get(), stmt);

            // Emit GROUP BY clause if present
            const auto& grouping_exprs = node->groupingExprs();
            if (!grouping_exprs.empty())
            {
                current_result_->writeOpcode(Opcode::GROUP_BY);
                current_result_->writeInt32(static_cast<uint32_t>(grouping_exprs.size()));

                for (auto* expr : grouping_exprs)
                {
                    generateExpression(expr);
                }
            }

            // Emit aggregate initialization
            current_result_->writeOpcode(Opcode::AGG_INIT);
            current_result_->writeInt32(static_cast<uint32_t>(node->aggregates().size()));

            // Emit each aggregate function
            for (auto* agg_expr : node->aggregates())
            {
                generateAggregateFunc(agg_expr);
            }

            // Emit HAVING clause if present
            if (node->havingClause())
            {
                current_result_->writeOpcode(Opcode::HAVING);
                generateExpression(node->havingClause());
            }

            // Emit finalization
            current_result_->writeOpcode(Opcode::AGG_FINALIZE);
        }

        void BytecodeGenerator::generateSortPlan(
            scratchbird::optimizer::SortNode *node,
            parser::SelectStmt *stmt)
        {
            // For sort plans, recursively generate child plan first
            auto child_plan = node->childPlan();
            generateJoinPlan(child_plan.get(), stmt);

            // Emit ORDER BY clause
            const auto& order_by_items = node->orderByItems();
            current_result_->writeOpcode(Opcode::ORDER_BY);
            current_result_->writeInt32(static_cast<uint32_t>(order_by_items.size()));

            for (const auto& item : order_by_items)
            {
                // Emit sort key marker
                current_result_->writeOpcode(Opcode::SORT_KEY);

                // Emit expression to sort by
                generateExpression(item.expr);

                // Emit sort direction
                if (item.order == parser::SortOrder::ASC)
                {
                    current_result_->writeOpcode(Opcode::SORT_ASC);
                }
                else
                {
                    current_result_->writeOpcode(Opcode::SORT_DESC);
                }

                // Emit NULLS ordering if specified
                if (item.nulls_order == parser::NullsOrder::NULLS_FIRST)
                {
                    current_result_->writeOpcode(Opcode::NULLS_FIRST);
                }
                else if (item.nulls_order == parser::NullsOrder::NULLS_LAST)
                {
                    current_result_->writeOpcode(Opcode::NULLS_LAST);
                }
            }
        }

        void BytecodeGenerator::generateLimitPlan(
            scratchbird::optimizer::LimitNode *node,
            parser::SelectStmt *stmt)
        {
            // For limit plans, recursively generate child plan first
            auto child_plan = node->childPlan();
            generateJoinPlan(child_plan.get(), stmt);

            // Emit LIMIT clause
            if (node->limitCount() >= 0)
            {
                current_result_->writeOpcode(Opcode::LIMIT);
                current_result_->writeInt64(static_cast<uint64_t>(node->limitCount()));
            }

            // Emit OFFSET clause
            if (node->offsetCount() >= 0)
            {
                current_result_->writeOpcode(Opcode::OFFSET);
                current_result_->writeInt64(static_cast<uint64_t>(node->offsetCount()));
            }
        }

        // Phase 1, Task 6.3: Window function bytecode generation

        void BytecodeGenerator::generateWindowPlan(
            scratchbird::optimizer::WindowNode *node,
            parser::SelectStmt *stmt)
        {
            // For window plans, recursively generate child plan first
            auto child_plan = node->child();
            generateJoinPlan(child_plan.get(), stmt);

            // Emit WINDOW marker
            current_result_->writeOpcode(Opcode::WINDOW);

            // Emit count of window functions
            const auto& window_functions = node->windowFunctions();
            current_result_->writeInt32(static_cast<uint32_t>(window_functions.size()));

            // Emit each window function
            for (const auto& win_func : window_functions)
            {
                generateWindowFunc(win_func);
            }
        }

        void BytecodeGenerator::generateWindowFunc(
            const optimizer::WindowNode::WindowFunction& win_func)
        {
            // Emit function type opcode
            switch (win_func.func)
            {
                case parser::WindowFunc::ROW_NUMBER:
                    current_result_->writeOpcode(Opcode::WIN_ROW_NUMBER);
                    break;
                case parser::WindowFunc::RANK:
                    current_result_->writeOpcode(Opcode::WIN_RANK);
                    break;
                case parser::WindowFunc::DENSE_RANK:
                    current_result_->writeOpcode(Opcode::WIN_DENSE_RANK);
                    break;
                case parser::WindowFunc::LAG:
                    current_result_->writeOpcode(Opcode::WIN_LAG);
                    break;
                case parser::WindowFunc::LEAD:
                    current_result_->writeOpcode(Opcode::WIN_LEAD);
                    break;
                case parser::WindowFunc::FIRST_VALUE:
                    current_result_->writeOpcode(Opcode::WIN_FIRST_VALUE);
                    break;
                case parser::WindowFunc::LAST_VALUE:
                    current_result_->writeOpcode(Opcode::WIN_LAST_VALUE);
                    break;
                case parser::WindowFunc::NTH_VALUE:
                    current_result_->writeOpcode(Opcode::WIN_NTH_VALUE);
                    break;
            }

            // Emit argument count
            current_result_->writeInt32(static_cast<uint32_t>(win_func.args.size()));

            // Emit each argument expression
            for (auto* arg_expr : win_func.args)
            {
                generateExpression(arg_expr);
            }

            // Emit window specification
            generateWindowSpec(win_func.window_spec);

            // Emit output column name
            current_result_->writeString(win_func.output_column);
        }

        void BytecodeGenerator::generateWindowSpec(const parser::WindowSpec *spec)
        {
            if (!spec)
            {
                // No window spec - emit empty marker
                current_result_->writeOpcode(Opcode::WINDOW_SPEC);
                current_result_->writeInt32(0); // No PARTITION BY columns
                current_result_->writeInt32(0); // No ORDER BY columns
                current_result_->writeInt32(0); // No frame clause
                return;
            }

            current_result_->writeOpcode(Opcode::WINDOW_SPEC);

            // Emit PARTITION BY clause
            const auto& partition_by = spec->partitionBy();
            current_result_->writeInt32(static_cast<uint32_t>(partition_by.size()));
            if (!partition_by.empty())
            {
                current_result_->writeOpcode(Opcode::PARTITION_BY);
                for (auto* expr : partition_by)
                {
                    generateExpression(expr);
                }
            }

            // Emit ORDER BY clause
            const auto& order_by = spec->orderBy();
            current_result_->writeInt32(static_cast<uint32_t>(order_by.size()));
            if (!order_by.empty())
            {
                current_result_->writeOpcode(Opcode::WINDOW_ORDER_BY);
                for (auto* expr : order_by)
                {
                    generateExpression(expr);
                    // TODO: Emit sort direction and nulls handling
                    // This would require WindowSpec to store OrderByItem info
                }
            }

            // Emit frame clause
            if (spec->hasFrame())
            {
                current_result_->writeInt32(1); // Has frame clause
                generateFrameClause(spec);
            }
            else
            {
                current_result_->writeInt32(0); // No frame clause
            }
        }

        void BytecodeGenerator::generateFrameClause(const parser::WindowSpec *spec)
        {
            current_result_->writeOpcode(Opcode::FRAME_CLAUSE);

            // Emit frame mode (ROWS or RANGE)
            if (spec->frameMode() == parser::FrameMode::ROWS)
            {
                current_result_->writeOpcode(Opcode::FRAME_ROWS);
            }
            else
            {
                current_result_->writeOpcode(Opcode::FRAME_RANGE);
            }

            // Emit frame start boundary
            const auto& start = spec->frameStart();
            switch (start.type)
            {
                case parser::FrameBoundaryType::UNBOUNDED_PRECEDING:
                    current_result_->writeOpcode(Opcode::FRAME_UNBOUNDED_PRECEDING);
                    break;
                case parser::FrameBoundaryType::PRECEDING:
                    current_result_->writeOpcode(Opcode::FRAME_PRECEDING);
                    generateExpression(start.offset);
                    break;
                case parser::FrameBoundaryType::CURRENT_ROW:
                    current_result_->writeOpcode(Opcode::FRAME_CURRENT_ROW);
                    break;
                case parser::FrameBoundaryType::FOLLOWING:
                    current_result_->writeOpcode(Opcode::FRAME_FOLLOWING);
                    generateExpression(start.offset);
                    break;
                case parser::FrameBoundaryType::UNBOUNDED_FOLLOWING:
                    current_result_->writeOpcode(Opcode::FRAME_UNBOUNDED_FOLLOWING);
                    break;
            }

            // Emit frame end boundary
            const auto& end = spec->frameEnd();
            switch (end.type)
            {
                case parser::FrameBoundaryType::UNBOUNDED_PRECEDING:
                    current_result_->writeOpcode(Opcode::FRAME_UNBOUNDED_PRECEDING);
                    break;
                case parser::FrameBoundaryType::PRECEDING:
                    current_result_->writeOpcode(Opcode::FRAME_PRECEDING);
                    generateExpression(end.offset);
                    break;
                case parser::FrameBoundaryType::CURRENT_ROW:
                    current_result_->writeOpcode(Opcode::FRAME_CURRENT_ROW);
                    break;
                case parser::FrameBoundaryType::FOLLOWING:
                    current_result_->writeOpcode(Opcode::FRAME_FOLLOWING);
                    generateExpression(end.offset);
                    break;
                case parser::FrameBoundaryType::UNBOUNDED_FOLLOWING:
                    current_result_->writeOpcode(Opcode::FRAME_UNBOUNDED_FOLLOWING);
                    break;
            }
        }

        // Visitor methods for window function AST nodes

        void BytecodeGenerator::visit(parser::WindowFuncExpr *node)
        {
            // This is called when generating direct (non-optimized) bytecode
            // The optimized path uses generateWindowFunc() instead
            current_result_->addError("Direct window function bytecode generation not yet supported");
        }

        void BytecodeGenerator::visit(parser::WindowSpec *node)
        {
            // This is called when generating direct (non-optimized) bytecode
            // The optimized path uses generateWindowSpec() instead
            current_result_->addError("Direct window spec bytecode generation not yet supported");
        }

        void BytecodeGenerator::visit(parser::JSONFuncExpr *node)
        {
            // Phase 1 Task 7: JSON function bytecode generation

            // Generate bytecode for arguments first (arguments go on stack in order)
            for (auto *arg : node->args())
            {
                arg->accept(this);
            }

            // Emit the appropriate JSON opcode
            Opcode json_opcode;
            switch (node->func())
            {
            case parser::JSONFunc::JSON_EXTRACT:
                json_opcode = Opcode::JSON_EXTRACT;
                break;
            case parser::JSONFunc::JSONB_EXTRACT_PATH:
                json_opcode = Opcode::JSONB_EXTRACT_PATH;
                break;
            case parser::JSONFunc::ARROW:
                json_opcode = Opcode::JSON_ARROW;
                break;
            case parser::JSONFunc::DOUBLE_ARROW:
                json_opcode = Opcode::JSON_DOUBLE_ARROW;
                break;
            case parser::JSONFunc::HASH_ARROW:
                json_opcode = Opcode::JSON_HASH_ARROW;
                break;
            case parser::JSONFunc::HASH_DOUBLE_ARROW:
                json_opcode = Opcode::JSON_HASH_DOUBLE_ARROW;
                break;
            case parser::JSONFunc::JSON_OBJECT:
                json_opcode = Opcode::JSON_OBJECT;
                break;
            case parser::JSONFunc::JSON_ARRAY:
                json_opcode = Opcode::JSON_ARRAY;
                break;
            case parser::JSONFunc::JSONB_BUILD_OBJECT:
                json_opcode = Opcode::JSONB_BUILD_OBJECT;
                break;
            case parser::JSONFunc::JSONB_BUILD_ARRAY:
                json_opcode = Opcode::JSONB_BUILD_ARRAY;
                break;
            case parser::JSONFunc::JSON_SET:
                json_opcode = Opcode::JSON_SET;
                break;
            case parser::JSONFunc::JSON_INSERT:
                json_opcode = Opcode::JSON_INSERT;
                break;
            case parser::JSONFunc::JSON_REMOVE:
                json_opcode = Opcode::JSON_REMOVE;
                break;
            case parser::JSONFunc::JSONB_SET:
                json_opcode = Opcode::JSONB_SET;
                break;
            default:
                current_result_->addError("Unknown JSON function in bytecode generation");
                return;
            }

            // Emit the JSON opcode
            current_result_->writeByte(static_cast<uint8_t>(json_opcode));

            // Emit argument count (helpful for executor to know how many args to pop)
            current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
        }

        void BytecodeGenerator::visit(parser::CoalesceExpr *node)
        {
            // Phase 1 Task 8: COALESCE bytecode generation
            // Generate bytecode for all arguments (they go on stack in order)
            for (auto *arg : node->args())
            {
                arg->accept(this);
            }

            // Emit COALESCE opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::COALESCE));

            // Emit argument count
            current_result_->writeByte(static_cast<uint8_t>(node->args().size()));
        }

        void BytecodeGenerator::visit(parser::NullIfExpr *node)
        {
            // Phase 1 Task 8: NULLIF bytecode generation
            // Generate bytecode for both arguments
            node->expr1()->accept(this);
            node->expr2()->accept(this);

            // Emit NULLIF opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::NULLIF));
        }

        void BytecodeGenerator::visit(parser::CaseExpr *node)
        {
            // Phase 1 Task 8: CASE bytecode generation
            // This generates a simple linearized bytecode that the executor will evaluate sequentially

            // Generate case operand if present (simple CASE)
            if (node->isSimpleCase())
            {
                node->caseOperand()->accept(this);
            }

            // Generate bytecode for all WHEN conditions and results
            for (const auto& when : node->whenClauses())
            {
                when.condition->accept(this);
                when.result->accept(this);
            }

            // Generate bytecode for ELSE result if present
            if (node->elseResult())
            {
                node->elseResult()->accept(this);
            }

            // Emit CASE_WHEN opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::CASE_WHEN));

            // Emit flags:
            // bit 0: has case operand (simple CASE)
            // bit 1: has else clause
            uint8_t flags = 0;
            if (node->isSimpleCase())
                flags |= 0x01;
            if (node->elseResult())
                flags |= 0x02;
            current_result_->writeByte(flags);

            // Emit WHEN clause count
            current_result_->writeByte(static_cast<uint8_t>(node->whenClauses().size()));
        }

        void BytecodeGenerator::visit(parser::ArrayLiteral *node)
        {
            // Phase 2 Task 12: ARRAY literal bytecode generation
            // Generate bytecode for all array elements
            for (auto *elem : node->elements())
            {
                elem->accept(this);
            }

            // Emit extended opcode for ARRAY construction
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ARRAY_CONSTRUCT));
            current_result_->writeByte(static_cast<uint8_t>(node->elements().size()));
        }

        void BytecodeGenerator::visit(parser::SubqueryExpr *node)
        {
            // Phase 2 Wave 2 - Agent B: Subquery bytecode generation

            // Emit extended opcode prefix
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));

            // Emit subquery type opcode
            switch (node->type())
            {
                case parser::SubqueryType::SCALAR:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SUBQUERY_SCALAR));
                    break;
                case parser::SubqueryType::EXISTS:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SUBQUERY_EXISTS));
                    break;
                case parser::SubqueryType::IN:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SUBQUERY_IN));
                    break;
                case parser::SubqueryType::NOT_IN:
                    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SUBQUERY_NOT_IN));
                    break;
                case parser::SubqueryType::ARRAY:
                    // ARRAY subqueries not yet implemented in executor
                    current_result_->addError("ARRAY subqueries not yet supported");
                    return;
            }

            // Generate bytecode for the subquery SELECT statement
            if (node->query())
            {
                node->query()->accept(this);
            }
            else
            {
                current_result_->addError("Subquery has no query statement");
            }

            // Emit subquery end marker
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_SUBQUERY_END));
        }

        void BytecodeGenerator::visit(parser::SequenceFunctionExpr *node)
        {
            // Generate bytecode for sequence functions (ALPHA Phase 1 - Sequences)

            // Emit opcode based on function type
            switch (node->functionType())
            {
                case parser::SequenceFunctionType::NEXTVAL:
                    current_result_->writeOpcode(Opcode::SEQUENCE_NEXTVAL);
                    break;
                case parser::SequenceFunctionType::CURRVAL:
                    current_result_->writeOpcode(Opcode::SEQUENCE_CURRVAL);
                    break;
                case parser::SequenceFunctionType::SETVAL:
                    current_result_->writeOpcode(Opcode::SEQUENCE_SETVAL);
                    break;
            }

            // Generate sequence name expression
            if (node->sequenceName())
            {
                node->sequenceName()->accept(this);
            }
            else
            {
                current_result_->addError("Sequence function missing sequence name");
            }

            // For SETVAL, also generate value and optional is_called flag
            if (node->functionType() == parser::SequenceFunctionType::SETVAL)
            {
                if (node->value())
                {
                    node->value()->accept(this);
                }
                else
                {
                    current_result_->addError("SETVAL missing value argument");
                }

                // is_called parameter (optional, defaults to true)
                if (node->isCalled())
                {
                    node->isCalled()->accept(this);
                }
                else
                {
                    // Default: true (mark as called) - use INT64 literal
                    current_result_->writeOpcode(Opcode::LITERAL_INT64);
                    current_result_->writeInt64(1);  // true = 1
                }
            }
        }

        // ===== Disassembler Implementation =====

        std::string BytecodeDisassembler::disassemble(const std::vector<uint8_t> &bytecode)
        {
            std::stringstream ss;
            size_t pos = 0;
            bool incomplete = false;

            while (pos < bytecode.size())
            {
                ss << std::setw(4) << std::setfill('0') << pos << ": ";

                Opcode op = static_cast<Opcode>(bytecode[pos]);
                ss << opcodeToString(op);
                pos++;

                // Handle operands based on opcode
                switch (op)
                {
                    case Opcode::VERSION:
                        if (pos < bytecode.size())
                        {
                            ss << " " << static_cast<int>(bytecode[pos]);
                            pos++;
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    case Opcode::LITERAL_INT32:
                        if (pos + 4 <= bytecode.size())
                        {
                            uint32_t val = readInt32(&bytecode[pos]);
                            ss << " " << val;
                            pos += 4;
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    case Opcode::LITERAL_INT64:
                        if (pos + 8 <= bytecode.size())
                        {
                            uint64_t val = readInt64(&bytecode[pos]);
                            ss << " " << val;
                            pos += 8;
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    case Opcode::LITERAL_DOUBLE:
                        if (pos + 8 <= bytecode.size())
                        {
                            double val;
                            memcpy(&val, &bytecode[pos], 8);
                            ss << " " << val;
                            pos += 8;
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    case Opcode::LITERAL_STRING:
                    case Opcode::TABLE_REF:
                    case Opcode::COLUMN_REF:
                        if (pos + 4 <= bytecode.size())
                        {
                            uint32_t len = readInt32(&bytecode[pos]);
                            pos += 4;
                            if (pos + len <= bytecode.size())
                            {
                                std::string str(reinterpret_cast<const char *>(&bytecode[pos]),
                                                len);
                                ss << " \"" << str << "\"";
                                pos += len;
                            }
                            else
                            {
                                ss << " <INCOMPLETE STRING>";
                                incomplete = true;
                            }
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    case Opcode::BEGIN_LIST:
                        if (pos + 4 <= bytecode.size())
                        {
                            uint32_t count = readInt32(&bytecode[pos]);
                            ss << " count=" << count;
                            pos += 4;
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    case Opcode::TYPE_VARCHAR:
                        if (pos + 4 <= bytecode.size())
                        {
                            uint32_t precision = readInt32(&bytecode[pos]);
                            ss << " (" << precision << ")";
                            pos += 4;
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    case Opcode::FUNC_LENGTH:
                    case Opcode::FUNC_SUBSTRING:
                    case Opcode::FUNC_UPPER:
                    case Opcode::FUNC_LOWER:
                    case Opcode::FUNC_TRIM:
                    case Opcode::AGG_SUM:
                    case Opcode::AGG_AVG:
                    case Opcode::AGG_MIN:
                    case Opcode::AGG_MAX:
                    case Opcode::AGG_COUNT:
                    case Opcode::FUNC_DATE_ADD:
                    case Opcode::FUNC_DATE_SUB:
                    case Opcode::FUNC_DATE_DIFF:
                    case Opcode::FUNC_NOW:
                    case Opcode::FUNC_CURRENT_DATE:
                        if (pos < bytecode.size())
                        {
                            uint8_t arg_count = bytecode[pos];
                            ss << " argc=" << static_cast<int>(arg_count);
                            pos++;
                        }
                        else
                        {
                            ss << " <INCOMPLETE>";
                            incomplete = true;
                        }
                        break;

                    default:
                        // No operands
                        break;
                }

                ss << "\n";
            }

            if (incomplete)
            {
                ss << "\nWARNING: Bytecode appears incomplete or malformed\n";
            }

            return ss.str();
        }

        std::string BytecodeDisassembler::opcodeToString(Opcode op)
        {
            switch (op)
            {
                case Opcode::END:
                    return "END";
                case Opcode::VERSION:
                    return "VERSION";
                case Opcode::CREATE_TABLE:
                    return "CREATE_TABLE";
                case Opcode::INSERT:
                    return "INSERT";
                case Opcode::SELECT:
                    return "SELECT";
                case Opcode::START_TRANSACTION:
                    return "START_TRANSACTION";
                case Opcode::SET_TRANSACTION:
                    return "SET_TRANSACTION";
                case Opcode::COMMIT:
                    return "COMMIT";
                case Opcode::ROLLBACK:
                    return "ROLLBACK";
                case Opcode::SWEEP:
                    return "SWEEP";
                case Opcode::TYPE_INTEGER:
                    return "TYPE_INTEGER";
                case Opcode::TYPE_BIGINT:
                    return "TYPE_BIGINT";
                case Opcode::TYPE_DOUBLE:
                    return "TYPE_DOUBLE";
                case Opcode::TYPE_VARCHAR:
                    return "TYPE_VARCHAR";
                case Opcode::LITERAL_NULL:
                    return "LITERAL_NULL";
                case Opcode::LITERAL_INT32:
                    return "LITERAL_INT32";
                case Opcode::LITERAL_INT64:
                    return "LITERAL_INT64";
                case Opcode::LITERAL_DOUBLE:
                    return "LITERAL_DOUBLE";
                case Opcode::LITERAL_STRING:
                    return "LITERAL_STRING";
                case Opcode::TABLE_REF:
                    return "TABLE_REF";
                case Opcode::COLUMN_REF:
                    return "COLUMN_REF";
                case Opcode::COLUMN_DEF:
                    return "COLUMN_DEF";
                case Opcode::EXPR_ADD:
                    return "EXPR_ADD";
                case Opcode::EXPR_SUBTRACT:
                    return "EXPR_SUBTRACT";
                case Opcode::EXPR_MULTIPLY:
                    return "EXPR_MULTIPLY";
                case Opcode::EXPR_DIVIDE:
                    return "EXPR_DIVIDE";
                case Opcode::EXPR_MODULO:
                    return "EXPR_MODULO";
                case Opcode::EXPR_EQ:
                    return "EXPR_EQ";
                case Opcode::EXPR_NE:
                    return "EXPR_NE";
                case Opcode::EXPR_LT:
                    return "EXPR_LT";
                case Opcode::EXPR_GT:
                    return "EXPR_GT";
                case Opcode::EXPR_LE:
                    return "EXPR_LE";
                case Opcode::EXPR_GE:
                    return "EXPR_GE";
                case Opcode::EXPR_AND:
                    return "EXPR_AND";
                case Opcode::EXPR_OR:
                    return "EXPR_OR";
                case Opcode::EXPR_CAST:
                    return "EXPR_CAST";
                case Opcode::EXPR_LIKE:
                    return "EXPR_LIKE";
                case Opcode::EXPR_ILIKE:
                    return "EXPR_ILIKE";
                case Opcode::FUNC_LENGTH:
                    return "FUNC_LENGTH";
                case Opcode::FUNC_SUBSTRING:
                    return "FUNC_SUBSTRING";
                case Opcode::FUNC_UPPER:
                    return "FUNC_UPPER";
                case Opcode::FUNC_LOWER:
                    return "FUNC_LOWER";
                case Opcode::FUNC_TRIM:
                    return "FUNC_TRIM";
                case Opcode::AGG_SUM:
                    return "AGG_SUM";
                case Opcode::AGG_AVG:
                    return "AGG_AVG";
                case Opcode::AGG_MIN:
                    return "AGG_MIN";
                case Opcode::AGG_MAX:
                    return "AGG_MAX";
                case Opcode::AGG_COUNT:
                    return "AGG_COUNT";
                case Opcode::FUNC_DATE_ADD:
                    return "FUNC_DATE_ADD";
                case Opcode::FUNC_DATE_SUB:
                    return "FUNC_DATE_SUB";
                case Opcode::FUNC_DATE_DIFF:
                    return "FUNC_DATE_DIFF";
                case Opcode::FUNC_NOW:
                    return "FUNC_NOW";
                case Opcode::FUNC_CURRENT_DATE:
                    return "FUNC_CURRENT_DATE";
                case Opcode::BEGIN_LIST:
                    return "BEGIN_LIST";
                case Opcode::END_LIST:
                    return "END_LIST";
                case Opcode::NOT_NULL:
                    return "NOT_NULL";
                case Opcode::SELECT_STAR:
                    return "SELECT_STAR";
                case Opcode::WHERE_CLAUSE:
                    return "WHERE_CLAUSE";
                default:
                    return "UNKNOWN";
            }
        }

        void BytecodeGenerator::visit(parser::ExtractExpr *node)
        {
            // Generate bytecode for EXTRACT(field FROM value)
            // Format: EXTENDED_OPCODE EXT_EXTRACT field_id source_expr

            // Emit extended opcode marker + EXT_EXTRACT
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_EXTRACT));

            // Emit field ID (uint8_t)
            current_result_->writeByte(node->fieldId());

            // Generate bytecode for source expression
            if (node->source())
            {
                node->source()->accept(this);
            }
            else
            {
                current_result_->addError("EXTRACT missing source expression");
            }
        }


        // Phase 2 Wave 2 - Agent C: Trigger bytecode generation
        void BytecodeGenerator::visit(parser::CreateTriggerStmt *node)
        {
            // Emit extended opcode marker
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_CREATE_TRIGGER));

            // Emit trigger name
            std::string_view trigger_name = string_pool_.get(node->triggerName());
            current_result_->writeInt32(static_cast<uint32_t>(trigger_name.length()));
            for (char c : trigger_name)
            {
                current_result_->writeByte(static_cast<uint8_t>(c));
            }

            // Emit table name
            std::string_view table_name = string_pool_.get(node->tableName());
            current_result_->writeInt32(static_cast<uint32_t>(table_name.length()));
            for (char c : table_name)
            {
                current_result_->writeByte(static_cast<uint8_t>(c));
            }

            // Emit timing (1 byte)
            current_result_->writeByte(static_cast<uint8_t>(node->timing()));

            // Emit event (1 byte)
            current_result_->writeByte(static_cast<uint8_t>(node->event()));

            // Emit granularity (1 byte)
            current_result_->writeByte(static_cast<uint8_t>(node->granularity()));

            // Emit procedure name
            std::string_view procedure_name = string_pool_.get(node->procedureName());
            current_result_->writeInt32(static_cast<uint32_t>(procedure_name.length()));
            for (char c : procedure_name)
            {
                current_result_->writeByte(static_cast<uint8_t>(c));
            }
        }

        void BytecodeGenerator::visit(parser::DropTriggerStmt *node)
        {
            // Emit extended opcode marker
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DROP_TRIGGER));

            // Emit trigger name
            std::string_view trigger_name = string_pool_.get(node->triggerName());
            current_result_->writeInt32(static_cast<uint32_t>(trigger_name.length()));
            for (char c : trigger_name)
            {
                current_result_->writeByte(static_cast<uint8_t>(c));
            }

            // Emit if_exists flag (1 byte)
            current_result_->writeByte(node->ifExists() ? 1 : 0);
        }

        // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2) =====

        void BytecodeGenerator::visit(parser::CreateFunctionStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_FUNCTION));

            // Emit function name
            writeStringId(node->name());

            // Emit parameter count
            current_result_->writeByte(static_cast<uint8_t>(node->parameters().size()));

            // Emit parameters
            for (const auto *param : node->parameters())
            {
                // Parameter mode (IN/OUT/INOUT)
                current_result_->writeByte(static_cast<uint8_t>(param->mode));
                // Parameter name
                writeStringId(param->name);
                // Parameter type
                writeDataType(*param->type);
            }

            // Emit return type
            writeDataType(*node->returnType());

            // Emit function body (block statement)
            if (node->body())
            {
                node->body()->accept(this);
            }
        }

        void BytecodeGenerator::visit(parser::CreateProcedureStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_PROCEDURE));

            // Emit procedure name
            writeStringId(node->name());

            // Emit parameter count
            current_result_->writeByte(static_cast<uint8_t>(node->parameters().size()));

            // Emit parameters
            for (const auto *param : node->parameters())
            {
                // Parameter mode (IN/OUT/INOUT)
                current_result_->writeByte(static_cast<uint8_t>(param->mode));
                // Parameter name
                writeStringId(param->name);
                // Parameter type
                writeDataType(*param->type);
            }

            // Emit procedure body (block statement)
            if (node->body())
            {
                node->body()->accept(this);
            }
        }

        void BytecodeGenerator::visit(parser::BlockStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_BLOCK));

            // Emit variable count
            current_result_->writeByte(static_cast<uint8_t>(node->declarations().size()));

            // Emit variable declarations
            for (auto *decl : node->declarations())
            {
                const_cast<parser::VarDeclarationStmt*>(decl)->accept(this);
            }

            // Emit statement count
            current_result_->writeInt32(static_cast<uint32_t>(node->statements().size()));

            // Emit statements
            for (auto *stmt : node->statements())
            {
                const_cast<parser::Statement*>(stmt)->accept(this);
            }

            // Emit exception handler count
            current_result_->writeByte(static_cast<uint8_t>(node->exceptionHandlers().size()));

            // Emit exception handlers
            for (const auto *handler : node->exceptionHandlers())
            {
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_EXCEPTION_HANDLER));
                writeStringId(handler->exception_name);

                // Emit handler statement count
                current_result_->writeInt32(static_cast<uint32_t>(handler->statements.size()));
                for (auto *stmt : handler->statements)
                {
                    const_cast<parser::Statement*>(stmt)->accept(this);
                }
            }
        }

        void BytecodeGenerator::visit(parser::VarDeclarationStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_DECLARE));

            // Emit variable name
            writeStringId(node->name());

            // Emit variable type
            writeDataType(*node->type());

            // Emit has_default flag
            current_result_->writeByte(node->defaultValue() ? 1 : 0);

            // Emit default value if present
            if (node->defaultValue())
            {
                generateExpression(node->defaultValue());
            }
        }

        void BytecodeGenerator::visit(parser::AssignmentStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_ASSIGN));

            // Note: Assignment implementation is stubbed - requires := operator in parser
            current_result_->addError("Assignment statements not yet fully implemented");
        }

        void BytecodeGenerator::visit(parser::IfStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_IF));

            // Generate condition
            generateExpression(node->condition());

            // Allocate label for end of IF
            int end_label = allocateLabel();
            int else_label = allocateLabel();

            // Jump to else/end if condition is false
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_JUMP_IF_FALSE));
            int jump_patch_offset = getCurrentOffset();
            current_result_->writeInt32(0);  // Placeholder for jump offset

            // Generate THEN statements
            for (auto *stmt : node->thenStatements())
            {
                const_cast<parser::Statement*>(stmt)->accept(this);
            }

            // Jump to end
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_JUMP));
            int end_jump_offset = getCurrentOffset();
            current_result_->writeInt32(0);  // Placeholder

            // Mark else position
            emitLabel(else_label);
            patchJump(else_label, jump_patch_offset);

            // Generate ELSIF clauses (stub)
            // TODO: Implement ELSIF generation

            // Generate ELSE statements
            for (auto *stmt : node->elseStatements())
            {
                const_cast<parser::Statement*>(stmt)->accept(this);
            }

            // Mark end position
            emitLabel(end_label);
            patchJump(end_label, end_jump_offset);
        }

        void BytecodeGenerator::visit(parser::LoopStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_LOOP));

            int loop_start = allocateLabel();
            emitLabel(loop_start);

            // Generate loop body
            for (auto *stmt : node->statements())
            {
                const_cast<parser::Statement*>(stmt)->accept(this);
            }

            // Jump back to start
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_JUMP));
            current_result_->writeInt32(getCurrentOffset() - 5);  // Jump back to loop start
        }

        void BytecodeGenerator::visit(parser::WhileStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_WHILE));

            int loop_start = allocateLabel();
            int loop_end = allocateLabel();

            emitLabel(loop_start);

            // Generate condition
            generateExpression(node->condition());

            // Jump to end if false
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_JUMP_IF_FALSE));
            int jump_offset = getCurrentOffset();
            current_result_->writeInt32(0);  // Placeholder

            // Generate loop body
            for (auto *stmt : node->statements())
            {
                const_cast<parser::Statement*>(stmt)->accept(this);
            }

            // Jump back to start
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_JUMP));
            current_result_->writeInt32(getCurrentOffset() - 5);

            // Mark end
            emitLabel(loop_end);
            patchJump(loop_end, jump_offset);
        }

        void BytecodeGenerator::visit(parser::ExitStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_EXIT));

            // Emit has_condition flag
            current_result_->writeByte(node->whenCondition() ? 1 : 0);

            // Generate condition if present
            if (node->whenCondition())
            {
                generateExpression(node->whenCondition());
            }
        }

        void BytecodeGenerator::visit(parser::ReturnStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_RETURN));

            // Emit has_value flag
            current_result_->writeByte(node->returnValue() ? 1 : 0);

            // Generate return value if present
            if (node->returnValue())
            {
                generateExpression(node->returnValue());
            }
        }

        void BytecodeGenerator::visit(parser::RaiseStmt *node)
        {
            // Emit extended opcode
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
            current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_RAISE));

            // Emit level (EXCEPTION/NOTICE/WARNING)
            current_result_->writeByte(static_cast<uint8_t>(node->level()));

            // Generate message expression
            if (node->message())
            {
                generateExpression(node->message());
            }
        }

        // PSQL control flow helpers
        int BytecodeGenerator::allocateLabel()
        {
            return next_label_id_++;
        }

        void BytecodeGenerator::patchJump(int label_id, int jump_offset)
        {
            // Record pending patch
            pending_patches_.push_back({jump_offset, label_id});
        }

        int BytecodeGenerator::getCurrentOffset() const
        {
            return static_cast<int>(current_result_->bytecode().size());
        }

        void BytecodeGenerator::emitLabel(int label_id)
        {
            label_positions_[label_id] = getCurrentOffset();

            // Patch any pending jumps to this label
            for (auto it = pending_patches_.begin(); it != pending_patches_.end(); )
            {
                if (it->second == label_id)
                {
                    // Patch the jump offset
                    int target_offset = label_positions_[label_id];
                    auto& bytecode = const_cast<std::vector<uint8_t>&>(current_result_->bytecode());
                    sblr::writeInt32(&bytecode[it->first], target_offset);
                    it = pending_patches_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // ============================================================================
        // INDEX OPERATION BYTECODE GENERATION HELPERS (November 19, 2025)
        // ============================================================================

        void BytecodeGenerator::writeIndexUUID(const uint8_t* uuid)
        {
            // Write 16-byte UUID
            for (int i = 0; i < 16; i++)
            {
                current_result_->writeByte(uuid[i]);
            }
        }

        void BytecodeGenerator::writeIndexType(IndexType type)
        {
            // Write index type as single byte
            current_result_->writeByte(static_cast<uint8_t>(type));
        }

        void BytecodeGenerator::writeKey(const std::vector<uint8_t>& key)
        {
            // Write key length as 2 bytes (little-endian)
            uint16_t key_len = static_cast<uint16_t>(key.size());
            current_result_->writeByte(key_len & 0xFF);
            current_result_->writeByte((key_len >> 8) & 0xFF);

            // Write key data
            for (uint8_t byte : key)
            {
                current_result_->writeByte(byte);
            }
        }

        void BytecodeGenerator::writeOptionalKey(const std::vector<uint8_t>* key)
        {
            // Write optional key (nullptr indicates unbounded, encoded as 0xFFFF)
            if (key == nullptr)
            {
                // Write 0xFFFF to indicate unbounded/null
                current_result_->writeByte(0xFF);
                current_result_->writeByte(0xFF);
            }
            else
            {
                // Write key length as 2 bytes (little-endian)
                uint16_t key_len = static_cast<uint16_t>(key->size());
                current_result_->writeByte(key_len & 0xFF);
                current_result_->writeByte((key_len >> 8) & 0xFF);

                // Write key data
                for (uint8_t byte : *key)
                {
                    current_result_->writeByte(byte);
                }
            }
        }

        void BytecodeGenerator::writeTID(uint64_t gpid, uint16_t slot)
        {
            // Write GPID (8 bytes, little-endian)
            for (int i = 0; i < 8; i++)
            {
                current_result_->writeByte((gpid >> (i * 8)) & 0xFF);
            }

            // Write slot (2 bytes, little-endian)
            current_result_->writeByte(slot & 0xFF);
            current_result_->writeByte((slot >> 8) & 0xFF);
        }

        void BytecodeGenerator::writeXid(uint64_t xid)
        {
            // Write transaction ID (8 bytes, little-endian)
            for (int i = 0; i < 8; i++)
            {
                current_result_->writeByte((xid >> (i * 8)) & 0xFF);
            }
        }

    } // namespace sblr
} // namespace scratchbird