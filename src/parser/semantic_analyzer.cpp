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

        // Helper function to check if a type is numeric
        static bool isNumericType(DataType type)
        {
            switch (type)
            {
            case DataType::INT8:
            case DataType::INT16:
            case DataType::INT32:
            case DataType::INT64:
            case DataType::INT128:
            case DataType::UINT8:
            case DataType::UINT16:
            case DataType::UINT32:
            case DataType::UINT64:
            case DataType::FLOAT32:
            case DataType::FLOAT64:
            case DataType::DECIMAL:
            case DataType::MONEY:
                return true;
            default:
                return false;
            }
        }

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

        void SemanticAnalyzer::visit(CreateIndexStmt *node)
        {
            // Phase 2 Task 2.3: CREATE INDEX semantic analysis
            // For now, minimal validation - full validation happens in executor
            // TODO: Add validation when semantic analyzer is fully integrated
            // - Check table exists
            // - Check columns exist in table
            // - Check index name not duplicate
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(CreateTablespaceStmt *node)
        {
            // Phase 2 Task 2.1: CREATE TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in CatalogManager
            // TODO: Add validation when tablespace catalog is integrated
            // - Check tablespace name not empty
            // - Check location path is absolute and valid
            // - Validate numeric parameters (autoextend_size, max_size, prealloc)
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(DropTablespaceStmt *node)
        {
            // Phase 2 Task 2.1: DROP TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in CatalogManager
            // TODO: Add validation when tablespace catalog is integrated
            // - Check tablespace exists
            // - Check FORCE clause requirements
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(AlterTablespaceStmt *node)
        {
            // Phase 2 Task 2.2: ALTER TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in CatalogManager
            // TODO: Add validation when tablespace catalog is integrated
            // - Check tablespace exists
            // - Validate alteration parameters (e.g., MAXSIZE >= current size)
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(AlterTableSetTablespaceStmt *node)
        {
            // Phase 4 Task 4.1.1: ALTER TABLE ... SET TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in executor
            // TODO: Add validation when migration logic is implemented
            // - Check table exists
            // - Check target tablespace exists
            // - Validate ONLINE clause (reject in Phase 4)
            (void)node; // Suppress unused parameter warning
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
            // Phase 1 Task 3.1: Handle JOINs in SELECT

            // Create new scope for column resolution
            symbol_table_.pushScope();

            // Resolve base table
            TableSymbol *base_table = resolveTable(node->fromClause().base_table.table_name);
            if (!base_table)
            {
                symbol_table_.popScope();
                return;
            }

            current_table_ = base_table;

            // Add base table columns to scope
            for (const auto &col : base_table->columns)
            {
                symbol_table_.addColumn(col.name, col);
            }

            // Process JOINs if present
            if (node->hasJoins())
            {
                for (const auto &join : node->fromClause().joins)
                {
                    // Resolve joined table
                    TableSymbol *join_table = resolveTable(join.right_table.table_name);
                    if (!join_table)
                        continue;

                    // Add joined table columns to scope
                    for (const auto &col : join_table->columns)
                    {
                        symbol_table_.addColumn(col.name, col);
                    }

                    // Validate join condition
                    if (join.condition_type == JoinConditionType::ON && join.on_condition)
                    {
                        checkExpression(join.on_condition);

                        // JOIN ON condition should be boolean
                        const ExpressionType *cond_type = getExpressionType(join.on_condition);
                        if (!cond_type || (cond_type->type.type != DataType::BOOLEAN &&
                                          cond_type->type.type != DataType::INT32))
                        {
                            reportError(join.on_condition, "JOIN ON condition must evaluate to boolean");
                        }
                    }
                    else if (join.condition_type == JoinConditionType::USING)
                    {
                        // Validate USING columns exist in both tables
                        for (auto col_id : join.using_columns)
                        {
                            const ColumnSymbol *col = resolveColumn(col_id);
                            if (!col)
                            {
                                std::string error_msg = "Column '";
                                error_msg += string_pool_.get(col_id);
                                error_msg += "' in USING clause does not exist";
                                reportError(node, error_msg);
                            }
                        }
                    }
                }
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

        void SemanticAnalyzer::visit(UpdateStmt *node)
        {
            // Phase 1 Task 2.1: UPDATE statement semantic analysis
            TableSymbol *table = resolveTable(node->tableName());
            if (!table)
                return;

            current_table_ = table;
            symbol_table_.pushScope();

            for (const auto &col : table->columns)
            {
                symbol_table_.addColumn(col.name, col);
            }

            // Process SET assignments
            for (const auto &assign : node->assignments())
            {
                const ColumnSymbol *col = resolveColumn(assign.column_name);
                if (!col)
                {
                    std::string error_msg = "Column '";
                    error_msg += string_pool_.get(assign.column_name);
                    error_msg += "' does not exist";
                    reportError(node, error_msg);
                    continue;
                }

                checkExpression(assign.value);
            }

            // Process WHERE clause
            if (node->whereClause())
            {
                checkExpression(node->whereClause());
            }

            symbol_table_.popScope();
            current_table_ = nullptr;
        }

        void SemanticAnalyzer::visit(DeleteStmt *node)
        {
            // Phase 1 Task 2.2: DELETE statement semantic analysis
            TableSymbol *table = resolveTable(node->tableName());
            if (!table)
                return;

            current_table_ = table;
            symbol_table_.pushScope();

            for (const auto &col : table->columns)
            {
                symbol_table_.addColumn(col.name, col);
            }

            // Process WHERE clause
            if (node->whereClause())
            {
                checkExpression(node->whereClause());
            }

            symbol_table_.popScope();
            current_table_ = nullptr;
        }

        void SemanticAnalyzer::visit(AnalyzeStmt *node)
        {
            // Phase 1 Task 1.1.2: ANALYZE statement semantic analysis
            // Validate that the table exists
            TableSymbol *table = resolveTable(node->tableName());
            if (!table)
                return;

            // If analyzing a specific column, validate it exists
            if (!node->analyzeAllColumns())
            {
                const ColumnSymbol *column = resolveColumn(node->columnName());
                if (!column)
                {
                    reportError(node, "Column not found in table");
                    return;
                }
            }

            // Sample rate validation already done in parser
            // No further semantic analysis needed
        }

        void SemanticAnalyzer::visit(ExplainStmt *node)
        {
            // Phase 1 Task 1.5: EXPLAIN statement semantic analysis
            // Analyze the query being explained
            if (node->query())
            {
                node->query()->accept(this);
            }
            // EXPLAIN itself has no additional validation
        }

        void SemanticAnalyzer::visit(AttachTablespaceStmt *node)
        {
            // Phase 6 Task 6.1: Attach tablespace - minimal semantic analysis
            // Tablespace validation will be done by the executor
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(DetachTablespaceStmt *node)
        {
            // Phase 6 Task 6.2: Detach tablespace - minimal semantic analysis
            // Tablespace validation will be done by the executor
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(StartTransactionStmt *node)
        {
            // Transaction statements don't require semantic analysis
            // They will be handled by the executor
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(SetTransactionStmt *node)
        {
            // Transaction statements don't require semantic analysis
            // They will be handled by the executor
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(CommitStmt *node)
        {
            // Transaction statements don't require semantic analysis
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(RollbackStmt *node)
        {
            // Transaction statements don't require semantic analysis
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(SweepStmt *node)
        {
            // Sweep statements don't require semantic analysis
            (void)node; // Suppress unused parameter warning
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
            // Phase 1 Task 3.1: Handle qualified column names (table.column)
            if (node->isQualified())
            {
                // Qualified identifier - need to find column in specific table
                // For now, just try to resolve the column name
                // TODO: Validate that the qualifier matches an available table/alias
                const ColumnSymbol *col = resolveColumn(node->name());
                if (col)
                {
                    setExpressionType(node, ExpressionType(col->type, col->nullable));
                }
                else
                {
                    std::string error_msg = "Column '";
                    error_msg += string_pool_.get(node->qualifier());
                    error_msg += ".";
                    error_msg += string_pool_.get(node->name());
                    error_msg += "' does not exist";
                    reportError(node, error_msg);
                    setExpressionType(node, ExpressionType());
                }
            }
            else
            {
                // Unqualified identifier - resolve column
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

                case BinaryOp::LIKE:
                case BinaryOp::ILIKE:
                    // LIKE/ILIKE operators - left is string, right is pattern
                    // No strict type checking needed, will convert at runtime
                    break;

                case BinaryOp::ARRAY_OVERLAP:
                case BinaryOp::ARRAY_CONTAINS:
                case BinaryOp::ARRAY_CONTAINED_BY:
                    // Array operators - expect JSON arrays
                    // No strict type checking needed, will validate at runtime
                    break;

                case BinaryOp::REGEX_MATCH:
                case BinaryOp::REGEX_MATCH_CI:
                case BinaryOp::REGEX_NOT_MATCH:
                case BinaryOp::REGEX_NOT_MATCH_CI:
                    // Regex operators - left is string, right is pattern
                    // No strict type checking needed, will convert at runtime
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

            // Set result type to target type (CAST always produces non-nullable unless explicitly
            // nullable)
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
            else if (func_name == "DATE_ADD")
            {
                if (node->args().size() != 2)
                {
                    reportError(node, "DATE_ADD expects 2 arguments (date, days)");
                    return;
                }
                checkExpression(node->args()[0]);
                checkExpression(node->args()[1]);
                // Result is DATE or TIMESTAMP
                setExpressionType(node, ExpressionType(TypeName(DataType::TIMESTAMP), false));
            }
            else if (func_name == "DATE_SUB")
            {
                if (node->args().size() != 2)
                {
                    reportError(node, "DATE_SUB expects 2 arguments (date, days)");
                    return;
                }
                checkExpression(node->args()[0]);
                checkExpression(node->args()[1]);
                // Result is DATE or TIMESTAMP
                setExpressionType(node, ExpressionType(TypeName(DataType::TIMESTAMP), false));
            }
            else if (func_name == "DATE_DIFF" || func_name == "DATEDIFF")
            {
                if (node->args().size() != 2)
                {
                    reportError(node, "DATE_DIFF expects 2 arguments (date1, date2)");
                    return;
                }
                checkExpression(node->args()[0]);
                checkExpression(node->args()[1]);
                // Result is INT64 (number of days)
                setExpressionType(node, ExpressionType(TypeName(DataType::INT64), false));
            }
            else if (func_name == "NOW")
            {
                if (node->args().size() != 0)
                {
                    reportError(node, "NOW expects 0 arguments");
                    return;
                }
                // Result is TIMESTAMP
                setExpressionType(node, ExpressionType(TypeName(DataType::TIMESTAMP), false));
            }
            else if (func_name == "CURRENT_DATE")
            {
                if (node->args().size() != 0)
                {
                    reportError(node, "CURRENT_DATE expects 0 arguments");
                    return;
                }
                // Result is DATE
                setExpressionType(node, ExpressionType(TypeName(DataType::DATE), false));
            }
            else
            {
                reportError(node, "Unknown function: " + std::string(func_name));
            }
        }

        void SemanticAnalyzer::visit(AggregateExpr *node)
        {
            // Check if we're in a valid context for aggregates
            if (in_aggregate_)
            {
                reportError(node, "Aggregate functions cannot be nested");
                return;
            }

            // Mark that we have aggregates
            has_aggregates_ = true;

            // Set in_aggregate flag to prevent nesting
            bool prev_in_agg = in_aggregate_;
            in_aggregate_ = true;

            // Check argument if present
            if (node->arg())
            {
                checkExpression(node->arg());

                // Get argument type
                const ExpressionType *arg_type = getExpressionType(node->arg());
                if (!arg_type)
                {
                    in_aggregate_ = prev_in_agg;
                    return;
                }

                // Type validation based on aggregate function
                switch (node->func())
                {
                case AggregateFunc::SUM:
                case AggregateFunc::AVG:
                    // Requires numeric type
                    if (!isNumericType(arg_type->type.type))
                    {
                        reportError(node, "SUM/AVG requires numeric argument");
                    }
                    break;
                case AggregateFunc::MIN:
                case AggregateFunc::MAX:
                    // Can work with any comparable type
                    break;
                case AggregateFunc::COUNT:
                    // COUNT can work with any type
                    break;
                }
            }

            // Restore flag
            in_aggregate_ = prev_in_agg;

            // Set result type based on aggregate function
            DataType result_type;
            switch (node->func())
            {
            case AggregateFunc::COUNT:
                result_type = DataType::INT64;  // COUNT always returns integer
                break;
            case AggregateFunc::SUM:
            case AggregateFunc::AVG:
                result_type = DataType::FLOAT64;  // Numeric aggregates return FLOAT64
                break;
            case AggregateFunc::MIN:
            case AggregateFunc::MAX:
                // MIN/MAX return same type as argument
                if (node->arg())
                {
                    const ExpressionType *arg_type = getExpressionType(node->arg());
                    if (arg_type)
                    {
                        setExpressionType(node, *arg_type);
                        return;
                    }
                }
                result_type = DataType::VARCHAR;  // Default fallback
                break;
            }

            setExpressionType(node, ExpressionType(TypeName(result_type), false));
        }

        void SemanticAnalyzer::visit(WindowFuncExpr *node)
        {
            // Phase 1 Task 6: Window function semantic analysis

            // Check arguments based on function type
            const auto& args = node->args();

            switch (node->func())
            {
            case WindowFunc::ROW_NUMBER:
            case WindowFunc::RANK:
            case WindowFunc::DENSE_RANK:
                // These functions take no arguments
                if (!args.empty())
                {
                    reportError(node, "ROW_NUMBER/RANK/DENSE_RANK take no arguments");
                }
                break;

            case WindowFunc::LAG:
            case WindowFunc::LEAD:
                // These functions require 1-3 arguments: (expr [, offset [, default]])
                if (args.empty() || args.size() > 3)
                {
                    reportError(node, "LAG/LEAD require 1-3 arguments");
                }
                else
                {
                    // Check first argument (value expression)
                    checkExpression(args[0]);

                    // Check second argument (offset) if present - must be integer
                    if (args.size() >= 2)
                    {
                        checkExpression(args[1]);
                        const ExpressionType* offset_type = getExpressionType(args[1]);
                        if (offset_type && !isNumericType(offset_type->type.type))
                        {
                            reportError(node, "LAG/LEAD offset must be numeric");
                        }
                    }

                    // Check third argument (default value) if present
                    if (args.size() >= 3)
                    {
                        checkExpression(args[2]);
                    }
                }
                break;

            case WindowFunc::FIRST_VALUE:
            case WindowFunc::LAST_VALUE:
                // These functions require exactly 1 argument
                if (args.size() != 1)
                {
                    reportError(node, "FIRST_VALUE/LAST_VALUE require exactly 1 argument");
                }
                else
                {
                    checkExpression(args[0]);
                }
                break;

            case WindowFunc::NTH_VALUE:
                // This function requires 2 arguments: (expr, n)
                if (args.size() != 2)
                {
                    reportError(node, "NTH_VALUE requires exactly 2 arguments");
                }
                else
                {
                    checkExpression(args[0]);
                    checkExpression(args[1]);

                    // Second argument must be integer
                    const ExpressionType* n_type = getExpressionType(args[1]);
                    if (n_type && !isNumericType(n_type->type.type))
                    {
                        reportError(node, "NTH_VALUE second argument must be numeric");
                    }
                }
                break;
            }

            // Check window specification
            if (node->windowSpec())
            {
                node->windowSpec()->accept(this);
            }

            // Set result type based on function
            DataType result_type;
            switch (node->func())
            {
            case WindowFunc::ROW_NUMBER:
            case WindowFunc::RANK:
            case WindowFunc::DENSE_RANK:
                // These always return INT64
                result_type = DataType::INT64;
                break;

            case WindowFunc::LAG:
            case WindowFunc::LEAD:
            case WindowFunc::FIRST_VALUE:
            case WindowFunc::LAST_VALUE:
            case WindowFunc::NTH_VALUE:
                // These return the type of their first argument
                if (!args.empty())
                {
                    const ExpressionType* arg_type = getExpressionType(args[0]);
                    if (arg_type)
                    {
                        setExpressionType(node, *arg_type);
                        return;
                    }
                }
                result_type = DataType::VARCHAR; // Fallback
                break;
            }

            setExpressionType(node, ExpressionType(TypeName(result_type), false));
        }

        void SemanticAnalyzer::visit(WindowSpec *node)
        {
            // Phase 1 Task 6: Window specification analysis

            // Check PARTITION BY expressions
            for (auto* expr : node->partitionBy())
            {
                checkExpression(expr);
            }

            // Check ORDER BY expressions
            for (auto* expr : node->orderBy())
            {
                checkExpression(expr);
            }

            // Check frame clause if present
            if (node->hasFrame())
            {
                // Validate frame boundaries
                const auto& start = node->frameStart();
                const auto& end = node->frameEnd();

                // Check start boundary offset expression
                if (start.offset)
                {
                    checkExpression(start.offset);
                    const ExpressionType* offset_type = getExpressionType(start.offset);
                    if (offset_type && !isNumericType(offset_type->type.type))
                    {
                        reportError(node, "Frame boundary offset must be numeric");
                    }
                }

                // Check end boundary offset expression
                if (end.offset)
                {
                    checkExpression(end.offset);
                    const ExpressionType* offset_type = getExpressionType(end.offset);
                    if (offset_type && !isNumericType(offset_type->type.type))
                    {
                        reportError(node, "Frame boundary offset must be numeric");
                    }
                }

                // Validate frame boundary order
                // Start must be before end (simplified check)
                if (start.type == FrameBoundaryType::UNBOUNDED_FOLLOWING)
                {
                    reportError(node, "Frame start cannot be UNBOUNDED FOLLOWING");
                }
                if (end.type == FrameBoundaryType::UNBOUNDED_PRECEDING)
                {
                    reportError(node, "Frame end cannot be UNBOUNDED PRECEDING");
                }
            }
        }

        void SemanticAnalyzer::visit(JSONFuncExpr *node)
        {
            // Phase 1 Task 7: JSON function analysis

            // Check all arguments
            for (auto* arg : node->args())
            {
                checkExpression(arg);
            }

            // Validate argument counts and types based on function
            switch (node->func())
            {
            case JSONFunc::JSON_EXTRACT:
            case JSONFunc::ARROW:
            case JSONFunc::DOUBLE_ARROW:
                // Require 2 arguments: json_value, path
                if (node->args().size() != 2)
                {
                    reportError(node, "JSON extraction requires 2 arguments: json value and path");
                }
                break;

            case JSONFunc::HASH_ARROW:
            case JSONFunc::HASH_DOUBLE_ARROW:
                // Require 2 arguments: json_value, path_array
                if (node->args().size() != 2)
                {
                    reportError(node, "JSON path extraction requires 2 arguments: json value and path array");
                }
                break;

            case JSONFunc::JSONB_EXTRACT_PATH:
                // Require at least 2 arguments: json_value, path_element1, ...
                if (node->args().size() < 2)
                {
                    reportError(node, "JSONB_EXTRACT_PATH requires at least 2 arguments");
                }
                break;

            case JSONFunc::JSON_SET:
            case JSONFunc::JSONB_SET:
                // Require 3 arguments: json_value, path, new_value
                if (node->args().size() != 3)
                {
                    reportError(node, "JSON_SET requires 3 arguments: json value, path, and new value");
                }
                break;

            case JSONFunc::JSON_INSERT:
                // Require 3 arguments: json_value, path, value
                if (node->args().size() != 3)
                {
                    reportError(node, "JSON_INSERT requires 3 arguments: json value, path, and value");
                }
                break;

            case JSONFunc::JSON_REMOVE:
                // Require 2 arguments: json_value, path
                if (node->args().size() != 2)
                {
                    reportError(node, "JSON_REMOVE requires 2 arguments: json value and path");
                }
                break;

            case JSONFunc::JSON_OBJECT:
            case JSONFunc::JSONB_BUILD_OBJECT:
                // Require even number of arguments (key-value pairs)
                if (node->args().size() % 2 != 0)
                {
                    reportError(node, "JSON_OBJECT requires even number of arguments (key-value pairs)");
                }
                break;

            case JSONFunc::JSON_ARRAY:
            case JSONFunc::JSONB_BUILD_ARRAY:
                // Can have any number of arguments
                break;
            }

            // Determine result type based on function
            DataType result_type;
            switch (node->func())
            {
            case JSONFunc::ARROW:
            case JSONFunc::HASH_ARROW:
            case JSONFunc::JSON_EXTRACT:
            case JSONFunc::JSON_OBJECT:
            case JSONFunc::JSON_ARRAY:
            case JSONFunc::JSON_SET:
            case JSONFunc::JSON_INSERT:
            case JSONFunc::JSON_REMOVE:
                // These return JSON type
                result_type = DataType::JSON;
                break;

            case JSONFunc::DOUBLE_ARROW:
            case JSONFunc::HASH_DOUBLE_ARROW:
                // These return TEXT
                result_type = DataType::TEXT;
                break;

            case JSONFunc::JSONB_EXTRACT_PATH:
            case JSONFunc::JSONB_BUILD_OBJECT:
            case JSONFunc::JSONB_BUILD_ARRAY:
            case JSONFunc::JSONB_SET:
                // These return JSONB type
                result_type = DataType::JSONB;
                break;
            }

            // JSON functions can return NULL
            setExpressionType(node, ExpressionType(TypeName(result_type), true));
        }

        void SemanticAnalyzer::visit(CoalesceExpr *node)
        {
            // Check all arguments
            for (auto *arg : node->args())
            {
                checkExpression(arg);
            }

            // COALESCE returns the type of the first non-null argument
            // For simplicity, use the type of the first argument
            if (!node->args().empty())
            {
                auto *first_type = getExpressionType(node->args()[0]);
                if (first_type)
                {
                    // COALESCE result is nullable if all arguments are nullable
                    setExpressionType(node, *first_type);
                }
                else
                {
                    // Default to INT if type unknown
                    setExpressionType(node, ExpressionType(TypeName(DataType::INT32), true));
                }
            }
        }

        void SemanticAnalyzer::visit(NullIfExpr *node)
        {
            // Check both arguments
            checkExpression(node->expr1());
            checkExpression(node->expr2());

            // NULLIF returns the same type as first argument, but always nullable
            auto *expr1_type = getExpressionType(node->expr1());
            if (expr1_type)
            {
                setExpressionType(node, ExpressionType(expr1_type->type, true));
            }
            else
            {
                // Default to INT if type unknown
                setExpressionType(node, ExpressionType(TypeName(DataType::INT32), true));
            }
        }

        void SemanticAnalyzer::visit(CaseExpr *node)
        {
            // Check case operand if present (simple CASE)
            if (node->isSimpleCase())
            {
                checkExpression(node->caseOperand());
            }

            // Check all WHEN conditions and results
            DataType result_type = DataType::INT32;  // Default type
            bool found_type = false;

            for (const auto& when : node->whenClauses())
            {
                checkExpression(when.condition);
                checkExpression(when.result);

                // Use the type of the first THEN result
                if (!found_type)
                {
                    auto *result_expr_type = getExpressionType(when.result);
                    if (result_expr_type)
                    {
                        result_type = result_expr_type->type.type;
                        found_type = true;
                    }
                }
            }

            // Check ELSE result if present
            if (node->elseResult())
            {
                checkExpression(node->elseResult());
            }

            // CASE result is nullable (could match no WHEN clause and have no ELSE)
            setExpressionType(node, ExpressionType(TypeName(result_type), true));
        }

        void SemanticAnalyzer::visit(ArrayLiteral *node)
        {
            // Check all array elements
            for (auto *elem : node->elements())
            {
                checkExpression(elem);
            }

            // Array type is JSON (arrays are stored as JSON)
            setExpressionType(node, ExpressionType(TypeName(DataType::JSON), false));
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