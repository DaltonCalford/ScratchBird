#include "scratchbird/parser/semantic_analyzer.h"
#include <sstream>
#include <unordered_set>

namespace scratchbird
{
    namespace parser
    {

        SemanticAnalyzer::SemanticAnalyzer(const StringPool &string_pool)
            : string_pool_(string_pool), current_result_(nullptr)
        {
        }

        SemanticAnalyzer::~SemanticAnalyzer() = default;

        SemanticResult SemanticAnalyzer::analyze(Statement *stmt)
        {
            SemanticResult result;
            current_result_ = &result;
            expression_types_.clear();

            // Visit the statement
            if (stmt)
            {
                stmt->accept(this);
            }
            else
            {
                reportError(SourceLocation(), "Null statement passed to analyzer");
            }

            current_result_ = nullptr;
            return result;
        }

        void SemanticAnalyzer::reportError(const SourceLocation &loc, const std::string &message)
        {
            if (current_result_)
            {
                current_result_->addError(SemanticError(loc, message));
            }
        }

        void SemanticAnalyzer::reportError(const ASTNode *node, const std::string &message)
        {
            reportError(node->span().start, message);
        }

        void SemanticAnalyzer::setExpressionType(Expression *expr, const ExpressionType &type)
        {
            expression_types_[expr] = type;
        }

        const ExpressionType *SemanticAnalyzer::getExpressionType(Expression *expr) const
        {
            auto it = expression_types_.find(expr);
            return (it != expression_types_.end()) ? &it->second : nullptr;
        }

        TableSymbol *SemanticAnalyzer::resolveTable(StringPool::StringId name)
        {
            TableSymbol *table = symbol_table_.findTable(name);
            if (!table)
            {
                std::stringstream ss;
                ss << "Table '" << string_pool_.get(name) << "' does not exist";
                reportError(SourceLocation(), ss.str());
            }
            return table;
        }

        const ColumnSymbol *SemanticAnalyzer::resolveColumn(StringPool::StringId name)
        {
            // First check current table context
            if (current_table_)
            {
                if (auto *col = current_table_->findColumn(name))
                {
                    return col;
                }
            }

            // Then check scope (for columns added to scope)
            if (auto *col = symbol_table_.findColumn(name))
            {
                return col;
            }

            std::stringstream ss;
            ss << "Column '" << string_pool_.get(name) << "' does not exist";
            reportError(SourceLocation(), ss.str());
            return nullptr;
        }

        // ===== Statement Visitors =====

        void SemanticAnalyzer::visit(CreateTableStmt *node)
        {
            // Check if table already exists
            if (symbol_table_.findTable(node->tableName()))
            {
                std::stringstream ss;
                ss << "Table '" << string_pool_.get(node->tableName()) << "' already exists";
                reportError(node, ss.str());
                return;
            }

            // Create table symbol
            auto table = std::make_unique<TableSymbol>(node->tableName());

            // Process columns
            std::unordered_set<StringPool::StringId> column_names;
            uint32_t column_index = 0;

            for (auto *col_def : node->columns())
            {
                // Check for duplicate column names
                if (!column_names.insert(col_def->name()).second)
                {
                    std::stringstream ss;
                    ss << "Duplicate column name '" << string_pool_.get(col_def->name()) << "'";
                    reportError(col_def, ss.str());
                    continue;
                }

                // Validate column definition
                col_def->accept(this);

                // Add column to table
                ColumnSymbol col_sym(col_def->name(), col_def->type(), col_def->nullable(),
                                     column_index++);
                table->addColumn(col_sym);
            }

            // Add table to symbol table
            symbol_table_.addTable(node->tableName(), std::move(table));
        }

        void SemanticAnalyzer::visit(InsertStmt *node)
        {
            // Resolve table
            TableSymbol *table = resolveTable(node->tableName());
            if (!table)
                return;

            // Check column count
            if (node->columns().size() != node->values().size())
            {
                reportError(node, "Column count doesn't match value count");
                return;
            }

            // Validate each column and value
            for (size_t i = 0; i < node->columns().size(); i++)
            {
                // Check if column exists
                const ColumnSymbol *col = table->findColumn(node->columns()[i]);
                if (!col)
                {
                    std::stringstream ss;
                    ss << "Column '" << string_pool_.get(node->columns()[i])
                       << "' does not exist in table '" << string_pool_.get(table->name) << "'";
                    reportError(node, ss.str());
                    continue;
                }

                // Type check the value expression
                Expression *value = node->values()[i];
                checkExpression(value);

                // Check type compatibility
                const ExpressionType *expr_type = getExpressionType(value);
                if (!expr_type || !TypeChecker::canAssign(col->type, expr_type->type))
                {
                    std::stringstream ss;
                    ss << "Cannot assign ";
                    if (expr_type)
                    {
                        ss << core::TypeSystem::getTypeName(expr_type->type.type);
                    }
                    else
                    {
                        ss << "unknown type";
                    }
                    ss << " to column '" << string_pool_.get(col->name) << "' of type ";
                    ss << core::TypeSystem::getTypeName(col->type.type);
                    reportError(value, ss.str());
                }

                // Check nullable constraint
                if (expr_type && !col->nullable && expr_type->is_nullable)
                {
                    std::stringstream ss;
                    ss << "Column '" << string_pool_.get(col->name) << "' cannot be NULL";
                    reportError(value, ss.str());
                }
            }
        }

        void SemanticAnalyzer::visit(SelectStmt *node)
        {
            // Resolve table
            TableSymbol *table = resolveTable(node->tableName());
            if (!table)
                return;

            // Set current table context
            current_table_ = table;

            // Create new scope for column resolution
            symbol_table_.pushScope();

            // Add all table columns to scope
            for (const auto &col : table->columns)
            {
                symbol_table_.addColumn(col.name, col);
            }

            // Process select list
            for (const auto &item : node->selectList())
            {
                if (item.is_star)
                {
                    // SELECT * - all columns are valid
                    continue;
                }

                // Type check the expression
                checkExpression(item.expr);
            }

            // Process WHERE clause if present
            if (node->whereClause())
            {
                checkExpression(node->whereClause());

                // WHERE clause must be boolean
                const ExpressionType *where_type = getExpressionType(node->whereClause());
                if (!where_type || (where_type->type.type != DataType::BOOLEAN &&
                                     where_type->type.type != DataType::INT32))
                {
                    reportError(node->whereClause(), "WHERE clause must evaluate to boolean");
                }
            }

            // Pop scope
            symbol_table_.popScope();
            current_table_ = nullptr;
        }

        // ===== Expression Visitors =====

        void SemanticAnalyzer::checkExpression(Expression *expr)
        {
            expr->accept(this);
        }

        void SemanticAnalyzer::visit(LiteralExpr *node)
        {
            ExpressionType type;

            switch (node->literalType())
            {
                case LiteralExpr::INTEGER:
                    type = ExpressionType(TypeName(DataType::INT32), false);
                    break;
                case LiteralExpr::FLOAT:
                    type = ExpressionType(TypeName(DataType::FLOAT64), false);
                    break;
                case LiteralExpr::STRING:
                    // Assume VARCHAR with unknown precision
                    type = ExpressionType(TypeName(DataType::VARCHAR, 0), false);
                    break;
                case LiteralExpr::NULL_LITERAL:
                    // NULL can be any type
                    type = ExpressionType(TypeName(DataType::NULL_TYPE), true);
                    break;
            }

            setExpressionType(node, type);
        }

        void SemanticAnalyzer::visit(IdentifierExpr *node)
        {
            // Resolve column
            const ColumnSymbol *col = resolveColumn(node->name());
            if (col)
            {
                setExpressionType(node, ExpressionType(col->type, col->nullable));
            }
            else
            {
                // Error already reported by resolveColumn
                // Set unknown type
                setExpressionType(node, ExpressionType());
            }
        }

        void SemanticAnalyzer::visit(BinaryOpExpr *node)
        {
            // Check both operands
            checkExpression(node->left());
            checkExpression(node->right());

            const ExpressionType *left_type = getExpressionType(node->left());
            const ExpressionType *right_type = getExpressionType(node->right());

            if (!left_type || !right_type)
            {
                // Can't determine types, report error
                reportError(node, "Cannot determine operand types");
                return;
            }

            // Check type compatibility
            switch (node->op())
            {
                case BinaryOp::ADD:
                case BinaryOp::SUBTRACT:
                case BinaryOp::MULTIPLY:
                case BinaryOp::DIVIDE:
                case BinaryOp::MODULO:
                    // Arithmetic operators
                    if (!TypeChecker::supportsArithmetic(left_type->type))
                    {
                        reportError(node->left(),
                                    "Left operand does not support arithmetic operations");
                    }
                    if (!TypeChecker::supportsArithmetic(right_type->type))
                    {
                        reportError(node->right(),
                                    "Right operand does not support arithmetic operations");
                    }
                    break;

                case BinaryOp::EQ:
                case BinaryOp::NE:
                case BinaryOp::LT:
                case BinaryOp::GT:
                case BinaryOp::LE:
                case BinaryOp::GE:
                    // Comparison operators
                    if (!TypeChecker::areCompatible(left_type->type, right_type->type))
                    {
                        reportError(node, "Operands are not type compatible for comparison");
                    }
                    break;

                case BinaryOp::AND:
                case BinaryOp::OR:
                    // Logical operators - accept BOOLEAN or INT32 (for compatibility)
                    if (left_type->type.type != DataType::BOOLEAN &&
                        left_type->type.type != DataType::INT32)
                    {
                        reportError(node->left(), "Left operand must be boolean");
                    }
                    if (right_type->type.type != DataType::BOOLEAN &&
                        right_type->type.type != DataType::INT32)
                    {
                        reportError(node->right(), "Right operand must be boolean");
                    }
                    break;
            }

            // Determine result type
            TypeName result_type =
                TypeChecker::getBinaryOpResultType(node->op(), left_type->type, right_type->type);
            bool result_nullable = left_type->is_nullable || right_type->is_nullable;

            setExpressionType(node, ExpressionType(result_type, result_nullable));
        }

        void SemanticAnalyzer::visit(CastExpr *node)
        {
            // Check the expression being cast
            checkExpression(node->expr());

            const ExpressionType *expr_type = getExpressionType(node->expr());
            if (!expr_type)
            {
                reportError(node, "Cannot determine type of expression being cast");
                return;
            }

            // Validate target type
            const TypeName &target_type = node->targetType();
            if (target_type.type == DataType::VARCHAR && target_type.precision == 0)
            {
                reportError(node, "VARCHAR type requires precision in CAST");
            }

            // Check if cast is valid using TypeSystem
            if (!core::TypeSystem::isExplicitlyConvertible(expr_type->type.type, target_type.type))
            {
                std::stringstream ss;
                ss << "Cannot cast from " << core::TypeSystem::getTypeName(expr_type->type.type)
                   << " to " << core::TypeSystem::getTypeName(target_type.type);
                reportError(node, ss.str());
            }

            // Set result type to target type (CAST always produces non-nullable unless explicitly nullable)
            setExpressionType(node, ExpressionType(target_type, false));
        }

        void SemanticAnalyzer::visit(FunctionCallExpr *node)
        {
            // Get function name
            std::string_view func_name = string_pool_.get(node->name());

            // Validate arguments for each function
            if (func_name == "LENGTH")
            {
                if (node->args().size() != 1)
                {
                    reportError(node, "LENGTH expects 1 argument");
                    return;
                }
                // Check argument
                checkExpression(node->args()[0]);
                // Result is INT32
                setExpressionType(node, ExpressionType(TypeName(DataType::INT32), false));
            }
            else if (func_name == "SUBSTRING")
            {
                if (node->args().size() != 3)
                {
                    reportError(node, "SUBSTRING expects 3 arguments (string, start, length)");
                    return;
                }
                // Check all arguments
                for (auto *arg : node->args())
                {
                    checkExpression(arg);
                }
                // Result is VARCHAR
                setExpressionType(node, ExpressionType(TypeName(DataType::VARCHAR, 255), false));
            }
            else if (func_name == "UPPER" || func_name == "LOWER")
            {
                if (node->args().size() != 1)
                {
                    reportError(node, std::string(func_name) + " expects 1 argument");
                    return;
                }
                checkExpression(node->args()[0]);
                // Result is VARCHAR
                setExpressionType(node, ExpressionType(TypeName(DataType::VARCHAR, 255), false));
            }
            else if (func_name == "TRIM")
            {
                if (node->args().size() != 1)
                {
                    reportError(node, "TRIM expects 1 argument");
                    return;
                }
                checkExpression(node->args()[0]);
                // Result is VARCHAR
                setExpressionType(node, ExpressionType(TypeName(DataType::VARCHAR, 255), false));
            }
            else if (func_name == "SUM")
            {
                if (node->args().size() != 1)
                {
                    reportError(node, "SUM expects 1 argument");
                    return;
                }
                checkExpression(node->args()[0]);
                // Result is numeric (use FLOAT64 for general compatibility)
                setExpressionType(node, ExpressionType(TypeName(DataType::FLOAT64), false));
            }
            else if (func_name == "AVG")
            {
                if (node->args().size() != 1)
                {
                    reportError(node, "AVG expects 1 argument");
                    return;
                }
                checkExpression(node->args()[0]);
                // Result is FLOAT64
                setExpressionType(node, ExpressionType(TypeName(DataType::FLOAT64), false));
            }
            else if (func_name == "MIN" || func_name == "MAX")
            {
                if (node->args().size() != 1)
                {
                    reportError(node, std::string(func_name) + " expects 1 argument");
                    return;
                }
                checkExpression(node->args()[0]);
                auto *arg_type = getExpressionType(node->args()[0]);
                if (arg_type)
                {
                    // MIN/MAX return same type as argument
                    setExpressionType(node, *arg_type);
                }
                else
                {
                    // Default to FLOAT64
                    setExpressionType(node, ExpressionType(TypeName(DataType::FLOAT64), false));
                }
            }
            else if (func_name == "COUNT")
            {
                if (node->args().size() != 1)
                {
                    reportError(node, "COUNT expects 1 argument");
                    return;
                }
                checkExpression(node->args()[0]);
                // Result is INT64
                setExpressionType(node, ExpressionType(TypeName(DataType::INT64), false));
            }
            else
            {
                reportError(node, "Unknown function: " + std::string(func_name));
            }
        }

        void SemanticAnalyzer::visit(ColumnDef *node)
        {
            // Validate column definition
            if (node->type().type == DataType::VARCHAR && node->type().precision == 0)
            {
                reportError(node, "VARCHAR type requires precision");
            }
        }

        // Convenience function
        std::unique_ptr<SemanticResult> analyzeAST(Statement *stmt, const StringPool &pool)
        {
            SemanticAnalyzer analyzer(pool);
            return std::make_unique<SemanticResult>(analyzer.analyze(stmt));
        }

    } // namespace parser
} // namespace scratchbird