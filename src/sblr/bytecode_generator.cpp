#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/debug.h"
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
        }

        void BytecodeGenerator::visit(parser::CreateIndexStmt *node)
        {
            // Generate CREATE INDEX bytecode (Phase 2 Task 2.3)
            current_result_->writeOpcode(Opcode::CREATE_INDEX);

            // Write index name
            writeStringId(node->indexName());

            // Write table name
            writeStringId(node->tableName());

            // Write is_unique flag (1 byte: 0 = non-unique, 1 = unique)
            current_result_->writeByte(node->isUnique() ? 1 : 0);

            // Write column count
            const auto &columns = node->columns();
            current_result_->writeInt32(static_cast<uint32_t>(columns.size()));

            // Write each column name
            for (auto column_id : columns)
            {
                writeStringId(column_id);
            }

            // Write tablespace name (Phase 2 Task 2.3)
            writeStringId(node->tablespace());
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
        }

        void BytecodeGenerator::visit(parser::SelectStmt *node)
        {
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

            // Write table name
            current_result_->writeOpcode(Opcode::TABLE_REF);
            writeStringId(node->tableName());

            // Write WHERE clause if present
            if (node->whereClause())
            {
                current_result_->writeOpcode(Opcode::WHERE_CLAUSE);
                // Null check handled in generateExpression
                generateExpression(node->whereClause());
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

    } // namespace sblr
} // namespace scratchbird