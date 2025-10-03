#include "scratchbird/sblr/bytecode_generator.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace scratchbird
{
    namespace sblr
    {

        BytecodeGenerator::BytecodeGenerator(const parser::StringPool &string_pool)
            : string_pool_(string_pool), current_result_(nullptr)
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
            current_result_->writeOpcode(Opcode::COLUMN_REF);
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