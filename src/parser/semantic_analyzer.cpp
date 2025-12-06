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
            // Phase 3 Enhancement: Add catalog-based validation when semantic analyzer is fully integrated
            // - Check table exists via catalog lookup
            // - Check columns exist in table
            // - Check index name not duplicate
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(CreateTablespaceStmt *node)
        {
            // Phase 2 Task 2.1: CREATE TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in CatalogManager
            // Phase 3 Enhancement: Add catalog-based validation when tablespace catalog is integrated
            // - Check tablespace name not empty
            // - Check location path is absolute and valid
            // - Validate numeric parameters (autoextend_size, max_size, prealloc)
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(DropTableStmt *node)
        {
            // ALPHA Phase 1: DROP TABLE semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(DropIndexStmt *node)
        {
            // ALPHA Phase 1: DROP INDEX semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(TruncateTableStmt *node)
        {
            // ALPHA Phase 1: TRUNCATE TABLE semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(AlterTableStmt *node)
        {
            // ALPHA Phase 1: ALTER TABLE semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(CreateSequenceStmt *node)
        {
            // ALPHA Phase 1: CREATE SEQUENCE semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(AlterSequenceStmt *node)
        {
            // ALPHA Phase 1: ALTER SEQUENCE semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(DropSequenceStmt *node)
        {
            // ALPHA Phase 1: DROP SEQUENCE semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(CreateViewStmt *node)
        {
            // ALPHA Phase 1: CREATE VIEW semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(DropViewStmt *node)
        {
            // ALPHA Phase 1: DROP VIEW semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(RefreshMaterializedViewStmt *node)
        {
            // ALPHA Phase 1: REFRESH MATERIALIZED VIEW semantic analysis
            // Minimal validation - full validation happens at execution
            (void)node;
        }

        void SemanticAnalyzer::visit(DropTablespaceStmt *node)
        {
            // Phase 2 Task 2.1: DROP TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in CatalogManager
            // Phase 3 Enhancement: Add catalog-based validation when tablespace catalog is integrated
            // - Check tablespace exists
            // - Check FORCE clause requirements
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(AlterTablespaceStmt *node)
        {
            // Phase 2 Task 2.2: ALTER TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in CatalogManager
            // Phase 3 Enhancement: Add catalog-based validation when tablespace catalog is integrated
            // - Check tablespace exists
            // - Validate alteration parameters (e.g., MAXSIZE >= current size)
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(AlterTableSetTablespaceStmt *node)
        {
            // Phase 4 Task 4.1.1: ALTER TABLE ... SET TABLESPACE semantic analysis
            // For now, minimal validation - full validation happens in executor
            // Phase 4 Enhancement: Add catalog-based validation when migration logic is implemented
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

            // Validate each row
            size_t row_num = 0;
            for (const auto& values : node->valueRows())
            {
                row_num++;

                // Check column count matches value count for this row
                if (node->columns().size() != values.size())
                {
                    std::stringstream ss;
                    ss << "Column count (" << node->columns().size()
                       << ") doesn't match value count (" << values.size()
                       << ") in row " << row_num;
                    reportError(node, ss.str());
                    continue;
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
                    Expression *value = values[i];
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
                        if (node->rowCount() > 1)
                        {
                            ss << " in row " << row_num;
                        }
                        reportError(value, ss.str());
                    }

                    // Check nullable constraint
                    if (expr_type && !col->nullable && expr_type->is_nullable)
                    {
                        std::stringstream ss;
                        ss << "Column '" << string_pool_.get(col->name) << "' cannot be NULL";
                        if (node->rowCount() > 1)
                        {
                            ss << " in row " << row_num;
                        }
                        reportError(value, ss.str());
                    }
                }
            }
        }

        void SemanticAnalyzer::visit(SelectStmt *node)
        {
            // Phase 1 Task 3.1: Handle JOINs in SELECT

            // Create new scope for column resolution
            symbol_table_.pushScope();

            // Phase 2 Wave 2: Process WITH clause (CTEs) if present
            if (node->withClause())
            {
                for (const auto &cte : node->withClause()->ctes())
                {
                    // Check CTE name is unique
                    if (resolveTable(cte.name))
                    {
                        std::string cte_name(string_pool_.get(cte.name));
                        reportError(node, "CTE name '" + cte_name + "' already defined");
                        continue;
                    }

                    // Analyze CTE query first
                    visit(cte.query);

                    // Create a table symbol for the CTE
                    auto cte_symbol = std::make_unique<TableSymbol>(cte.name);

                    // If column aliases provided, use them; otherwise use CTE query column names
                    if (!cte.column_aliases.empty())
                    {
                        // For now, just create placeholder columns with the provided aliases
                        // Full type inference would require examining the CTE query's select list
                        for (size_t i = 0; i < cte.column_aliases.size(); ++i)
                        {
                            ColumnSymbol col;
                            col.name = cte.column_aliases[i];
                            col.type = TypeName(DataType::INT32); // Placeholder type
                            col.nullable = true;
                            cte_symbol->columns.push_back(col);
                        }
                    }
                    else
                    {
                        // Extract column names and types from CTE select list
                        // For now, create a placeholder column
                        // Phase 3 Enhancement: Extract actual column info from CTE select list
                        // by analyzing the CTE's SelectStmt to determine result column types
                        ColumnSymbol col;
                        col.name = cte.name; // Use CTE name as fallback
                        col.type = TypeName(DataType::INT32);
                        col.nullable = true;
                        cte_symbol->columns.push_back(col);
                    }

                    // Add CTE as a table symbol in current scope
                    symbol_table_.addTable(cte.name, std::move(cte_symbol));
                }
            }

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

        void SemanticAnalyzer::visit(SetOperationStmt *node)
        {
            // Semantic analysis for set operations (UNION, INTERSECT, EXCEPT)
            // Both sides must be analyzed and have compatible schemas

            // Analyze left side
            node->left()->accept(this);

            // Analyze right side
            node->right()->accept(this);

            // Note: Full schema compatibility checking could be added here
            // For now, the executor will validate column count matches
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

        void SemanticAnalyzer::visit(MergeStmt *node)
        {
            // ALPHA Phase 1 - Advanced SQL: MERGE statement semantic analysis
            // Phase 3 Enhancement: Implement full semantic analysis for MERGE statement
            // - Validate target table exists
            // - Validate source table/subquery exists
            // - Validate WHEN MATCHED/NOT MATCHED clauses
            // - Check column types match in UPDATE SET expressions
            // For now, validation happens in executor
            (void)node;  // Suppress unused parameter warning
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

        void SemanticAnalyzer::visit(SavepointStmt *node)
        {
            // Savepoint statements don't require semantic analysis
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(ReleaseSavepointStmt *node)
        {
            // Release savepoint statements don't require semantic analysis
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(RollbackToSavepointStmt *node)
        {
            // Rollback to savepoint statements don't require semantic analysis
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(SweepStmt *node)
        {
            // Sweep statements don't require semantic analysis
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(ShowStmt *node)
        {
            // SHOW commands (TABLES, DATABASES, COLUMNS, INDEXES, CREATE TABLE)
            // No semantic validation required - these are metadata queries
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(DescribeStmt *node)
        {
            // DESCRIBE table command
            // No semantic validation required - this is a metadata query
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
                // Phase 3 Enhancement: Validate that the qualifier matches an available table/alias
                // by checking the symbol table for a table with the qualifier name
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

        void SemanticAnalyzer::visit(SequenceFunctionExpr *node)
        {
            // ALPHA Phase 1: Sequence function semantic analysis
            // Validate sequence name and arguments

            // All sequence functions return INT64
            setExpressionType(node, ExpressionType(TypeName(DataType::INT64), false));
        }

        void SemanticAnalyzer::visit(ExtractExpr *node)
        {
            // EXTRACT(field FROM value) semantic analysis
            // Validate source expression
            if (node->source())
            {
                node->source()->accept(this);
            }

            // EXTRACT returns INT32 for most fields, INT64 for epoch, FLOAT64 for some INTERVAL epoch
            // For simplicity, we'll type it as INT64 (can hold all integer results)
            // Phase 3 Enhancement: Could be more precise based on field type
            // (YEAR/MONTH/DAY -> INT32, EPOCH -> INT64, INTERVAL parts -> FLOAT64)
            setExpressionType(node, ExpressionType(TypeName(DataType::INT64), false));
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

        void SemanticAnalyzer::visit(GroupingExpr *node)
        {
            // GROUPING() returns 0 or 1 (INT) to indicate if a column is aggregated
            // Check the argument expression
            checkExpression(node->arg());

            // GROUPING() always returns INTEGER (0 or 1)
            setExpressionType(node, ExpressionType(TypeName(DataType::INT32), false));
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

        void SemanticAnalyzer::visit(SubqueryExpr *node)
        {
            // Phase 2 Wave 2 - Agent B: Subquery implementation

            // Save outer context for correlated subquery detection
            // (For now, we don't track correlation - will be enhanced later)

            // Analyze the subquery SELECT statement
            if (!node->query())
            {
                reportError(node, "Subquery has no query statement");
                setExpressionType(node, ExpressionType(TypeName(DataType::INT32), true));
                return;
            }

            // Visit the subquery
            visit(node->query());

            // Validate based on subquery type
            switch (node->type())
            {
                case SubqueryType::SCALAR:
                {
                    // Scalar subquery must return exactly one column
                    const auto& select_list = node->query()->selectList();
                    if (select_list.size() != 1)
                    {
                        reportError(node, "Scalar subquery must return exactly one column");
                        setExpressionType(node, ExpressionType(TypeName(DataType::INT32), true));
                        return;
                    }

                    // Get the type of the single column
                    const ExpressionType* col_type = getExpressionType(select_list[0].expr);
                    if (col_type)
                    {
                        // Scalar subquery can return NULL if no rows match
                        setExpressionType(node, ExpressionType(col_type->type, true));
                    }
                    else
                    {
                        // Fallback if type unknown
                        setExpressionType(node, ExpressionType(TypeName(DataType::INT32), true));
                    }
                    break;
                }

                case SubqueryType::EXISTS:
                {
                    // EXISTS always returns boolean
                    setExpressionType(node, ExpressionType(TypeName(DataType::BOOLEAN), false));
                    break;
                }

                case SubqueryType::IN:
                case SubqueryType::NOT_IN:
                {
                    // IN/NOT IN subquery must return exactly one column
                    const auto& select_list = node->query()->selectList();
                    if (select_list.size() != 1)
                    {
                        reportError(node, "IN subquery must return exactly one column");
                    }

                    // IN/NOT IN returns boolean
                    // Note: Type compatibility with left operand will be checked in BinaryOpExpr visitor
                    setExpressionType(node, ExpressionType(TypeName(DataType::BOOLEAN), true));
                    break;
                }

                case SubqueryType::ARRAY:
                {
                    // ARRAY subquery returns a JSON array
                    setExpressionType(node, ExpressionType(TypeName(DataType::JSON), false));
                    break;
                }
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

        // Phase 2 Wave 2 - Agent C: Trigger visitors
        void SemanticAnalyzer::visit(CreateTriggerStmt *node)
        {
            // Validate that the table exists
            // Note: Full validation would require catalog access,
            // which is typically done at execution time.
            // For now, we just validate basic syntax constraints
            
            // Trigger name should not be empty (parser ensures this)
            // Table name should not be empty (parser ensures this)
            // Procedure name should not be empty (parser ensures this)
            
            // All validation is syntactic at parse time
            // Semantic validation (table exists, procedure exists) happens at execution
        }
        
        void SemanticAnalyzer::visit(DropTriggerStmt *node)
        {
            // Validate DROP TRIGGER statement
            // Trigger name should not be empty (parser ensures this)

            // Semantic validation (trigger exists) happens at execution time
        }

        void SemanticAnalyzer::visit(CreateDatabaseTriggerStmt *node)
        {
            // Validate CREATE DATABASE TRIGGER statement
            // Trigger name should not be empty (parser ensures this)
            // Procedure name should not be empty (parser ensures this)
            // Position should be non-negative (parser ensures this via int32_t)

            // Semantic validation (procedure exists) happens at execution time
        }

        // ALPHA Phase 1 - User Defined Types
        void SemanticAnalyzer::visit(CreateTypeStmt *node)
        {
            // Validate CREATE TYPE statement
            // Type name should not be empty (parser ensures this)
            // For COMPOSITE types, at least one field is required
            // For ENUM types, at least one value is required
            // For RANGE types, subtype must be specified

            // All deeper validation happens at execution time
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(CreateDomainStmt *node)
        {
            // Validate CREATE DOMAIN statement
            // Domain name and base type should not be empty (parser ensures this)
            // CHECK constraint expression, if present, is syntactically valid

            // All deeper validation happens at execution time
            (void)node; // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(CallStmt *node)
        {
            // Validate CALL statement
            // Procedure name should not be empty (parser ensures this)
            // Arguments are expressions which may need semantic analysis

            // All deeper validation (procedure exists, argument types match) happens at execution time
            (void)node; // Suppress unused parameter warning
        }

        // Phase 2 Task 10.2 - PSQL visitors
        void SemanticAnalyzer::visit(CreateFunctionStmt *node)
        {
            // Function name validation done at parse time

            // Validate return type exists
            if (!node->returnType())
            {
                current_result_->addError(SemanticError(SourceLocation(), "Function must have a return type"));
                return;
            }

            // Validate parameters
            for (const auto* param : node->parameters())
            {
                // Parameter name validation done at parse time
                if (!param->type)
                {
                    current_result_->addError(SemanticError(SourceLocation(), "Parameter must have a type"));
                }
            }

            // Validate body exists
            if (node->body())
            {
                node->body()->accept(this);
            }
        }

        void SemanticAnalyzer::visit(CreateProcedureStmt *node)
        {
            // Procedure name validation done at parse time

            // Validate parameters
            for (const auto* param : node->parameters())
            {
                // Parameter name validation done at parse time
                if (!param->type)
                {
                    current_result_->addError(SemanticError(SourceLocation(), "Parameter must have a type"));
                }
            }

            // Validate body exists
            if (node->body())
            {
                node->body()->accept(this);
            }
        }

        void SemanticAnalyzer::visit(BlockStmt *node)
        {
            // Validate variable declarations
            for (const auto* decl : node->declarations())
            {
                if (decl)
                {
                    const_cast<VarDeclarationStmt*>(decl)->accept(this);
                }
            }

            // Validate statements
            for (const auto* stmt : node->statements())
            {
                if (stmt)
                {
                    const_cast<Statement*>(stmt)->accept(this);
                }
            }

            // Validate exception handlers (if present)
            for (const auto* handler : node->exceptionHandlers())
            {
                // Basic validation - handler exists
                if (!handler)
                {
                    current_result_->addError(SemanticError(SourceLocation(), "Invalid exception handler"));
                }
            }
        }

        void SemanticAnalyzer::visit(VarDeclarationStmt *node)
        {
            // Variable name validation done at parse time

            // Validate type
            if (!node->type())
            {
                current_result_->addError(SemanticError(SourceLocation(), "Variable must have a type"));
                return;
            }

            // If default value exists, validate it
            if (node->defaultValue())
            {
                node->defaultValue()->accept(this);
            }
        }

        void SemanticAnalyzer::visit(AssignmentStmt *node)
        {
            // Assignment target validation done at parse time

            // Validate value expression
            if (node->value())
            {
                node->value()->accept(this);
            }
            else
            {
                current_result_->addError(SemanticError(SourceLocation(), "Assignment must have a value expression"));
            }
        }

        void SemanticAnalyzer::visit(IfStmt *node)
        {
            // Validate condition
            if (node->condition())
            {
                node->condition()->accept(this);
            }
            else
            {
                current_result_->addError(SemanticError(SourceLocation(), "IF statement must have a condition"));
            }

            // Validate THEN statements
            for (const auto* stmt : node->thenStatements())
            {
                if (stmt)
                {
                    const_cast<Statement*>(stmt)->accept(this);
                }
            }

            // Validate ELSIF clauses
            for (const auto* elsif : node->elsifClauses())
            {
                if (elsif)
                {
                    if (elsif->condition)
                    {
                        elsif->condition->accept(this);
                    }
                    for (const auto* stmt : elsif->statements)
                    {
                        if (stmt)
                        {
                            const_cast<Statement*>(stmt)->accept(this);
                        }
                    }
                }
            }

            // Validate ELSE statements
            for (const auto* stmt : node->elseStatements())
            {
                if (stmt)
                {
                    const_cast<Statement*>(stmt)->accept(this);
                }
            }
        }

        void SemanticAnalyzer::visit(LoopStmt *node)
        {
            // Validate body statements
            for (const auto* stmt : node->statements())
            {
                if (stmt)
                {
                    const_cast<Statement*>(stmt)->accept(this);
                }
            }
        }

        void SemanticAnalyzer::visit(WhileStmt *node)
        {
            // Validate condition
            if (node->condition())
            {
                node->condition()->accept(this);
            }
            else
            {
                current_result_->addError(SemanticError(SourceLocation(), "WHILE statement must have a condition"));
            }

            // Validate body statements
            for (const auto* stmt : node->statements())
            {
                if (stmt)
                {
                    const_cast<Statement*>(stmt)->accept(this);
                }
            }
        }

        void SemanticAnalyzer::visit(ExitStmt *node)
        {
            // Validate WHEN condition if present
            if (node->whenCondition())
            {
                node->whenCondition()->accept(this);
            }

            // Label validation would require tracking active loops
            // For now, just ensure label ID is valid if present
            // (actual label existence check would be done at runtime)
        }

        void SemanticAnalyzer::visit(ReturnStmt *node)
        {
            // Validate return value if present
            if (node->returnValue())
            {
                node->returnValue()->accept(this);
            }

            // Type checking (return value type vs function return type)
            // would require additional context about current function
            // For now, just validate the expression is well-formed
        }

        void SemanticAnalyzer::visit(RaiseStmt *node)
        {
            // Validate message expression
            if (node->message())
            {
                node->message()->accept(this);
            }
            else
            {
                current_result_->addError(SemanticError(SourceLocation(), "RAISE statement must have a message"));
            }

            // Validate argument expressions
            for (const auto* arg : node->args())
            {
                if (arg)
                {
                    const_cast<Expression*>(arg)->accept(this);
                }
            }
        }

        // ===== Security Statement Visitors (ALPHA Phase 1 - Security System Phase 2) =====
        // Stubs - semantic analysis for security statements is minimal (name validation only)

        void SemanticAnalyzer::visit(CreateUserStmt *node)
        {
            // Minimal validation - username should be valid identifier
            // Password validation happens at execution time
            (void)node;  // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(AlterUserStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(DropUserStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(CreateRoleStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(DropRoleStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(CreateGroupStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(DropGroupStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(GrantPrivilegeStmt *node)
        {
            // Security Phase 3.3.3: Validate column-level permissions
            if (node->hasColumnList())
            {
                // Column lists are only valid for TABLE object type
                if (node->objectType() != GrantPrivilegeStmt::ObjectType::TABLE)
                {
                    current_result_->addError(SemanticError(
                        node->span().start,
                        "Column-level permissions can only be granted on TABLEs"));
                    return;
                }

                // Only SELECT, UPDATE, INSERT, and REFERENCES are valid for column-level grants
                uint32_t valid_col_privs =
                    static_cast<uint32_t>(GrantPrivilegeStmt::PrivilegeType::SELECT) |
                    static_cast<uint32_t>(GrantPrivilegeStmt::PrivilegeType::UPDATE) |
                    static_cast<uint32_t>(GrantPrivilegeStmt::PrivilegeType::INSERT) |
                    static_cast<uint32_t>(GrantPrivilegeStmt::PrivilegeType::REFERENCES);

                if ((node->privileges() & ~valid_col_privs) != 0)
                {
                    current_result_->addError(SemanticError(
                        node->span().start,
                        "Only SELECT, UPDATE, INSERT, and REFERENCES privileges can be granted on columns"));
                    return;
                }

                // Note: We cannot validate that columns exist here because we don't have
                // access to the catalog during semantic analysis. This will be validated
                // at execution time by the executor.
            }
        }

        void SemanticAnalyzer::visit(RevokePrivilegeStmt *node)
        {
            // Security Phase 3.3.3: Validate column-level permissions
            if (node->hasColumnList())
            {
                // Column lists are only valid for TABLE object type
                if (node->objectType() != RevokePrivilegeStmt::ObjectType::TABLE)
                {
                    current_result_->addError(SemanticError(
                        node->span().start,
                        "Column-level permissions can only be revoked on TABLEs"));
                    return;
                }

                // Only SELECT, UPDATE, INSERT, and REFERENCES are valid for column-level revokes
                uint32_t valid_col_privs =
                    static_cast<uint32_t>(RevokePrivilegeStmt::PrivilegeType::SELECT) |
                    static_cast<uint32_t>(RevokePrivilegeStmt::PrivilegeType::UPDATE) |
                    static_cast<uint32_t>(RevokePrivilegeStmt::PrivilegeType::INSERT) |
                    static_cast<uint32_t>(RevokePrivilegeStmt::PrivilegeType::REFERENCES);

                if ((node->privileges() & ~valid_col_privs) != 0)
                {
                    current_result_->addError(SemanticError(
                        node->span().start,
                        "Only SELECT, UPDATE, INSERT, and REFERENCES privileges can be revoked on columns"));
                    return;
                }

                // Note: Column existence validation happens at execution time.
            }
        }

        void SemanticAnalyzer::visit(GrantRoleStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(RevokeRoleStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(SetRoleStmt *node)
        {
            (void)node;
        }

        void SemanticAnalyzer::visit(SetSessionAuthStmt *node)
        {
            (void)node;
        }

        // P2-7: SET CONSTRAINTS statement
        void SemanticAnalyzer::visit(SetConstraintsStmt *node)
        {
            // Basic validation: if not ALL, must have at least one constraint name
            if (!node->allConstraints() && node->constraintNames().empty())
            {
                reportError(node, "SET CONSTRAINTS requires ALL or at least one constraint name");
            }
        }

        // Firebird ISQL compatibility: SET SQL DIALECT, SET NAMES, SET LOCAL_TIMEOUT
        void SemanticAnalyzer::visit(SetSqlDialectStmt *node)
        {
            // Dialect is already validated in the parser (1, 2, or 3)
            // Additional semantic validation could be added here if needed
            (void)node;  // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(SetNamesStmt *node)
        {
            // Charset name validation would require catalog access
            // For now, accept any identifier - runtime will validate
            (void)node;  // Suppress unused parameter warning
        }

        void SemanticAnalyzer::visit(SetLocalTimeoutStmt *node)
        {
            // Timeout is already validated in the parser (non-negative integer)
            (void)node;  // Suppress unused parameter warning
        }

        // Security Phase 3.4: Row-level security policy statements
        // P2-16: Policy Expression Validation at CREATE POLICY time
        void SemanticAnalyzer::visit(CreatePolicyStmt *node)
        {
            // Policy name and table name are required by grammar, so they're always present
            // if parsing succeeded. Just validate expressions.

            // P2-16: Validate USING expression if present
            if (node->hasUsingExpr() && node->usingExpr())
            {
                // The USING expression must be a boolean predicate
                // Visit the expression to validate column references and types
                Expression *using_expr = node->usingExpr();

                // Validate the expression - check for valid structure
                // Note: Full type checking would require table context from catalog
                // For now, validate that the expression is syntactically valid
                // LiteralExpr with non-boolean type is invalid for USING clause
                if (using_expr->kind() == ASTKind::LITERAL)
                {
                    auto *lit = static_cast<LiteralExpr *>(using_expr);
                    // NULL literals are acceptable (will evaluate to false)
                    // Non-boolean literals should be flagged
                    if (lit->literalType() != LiteralExpr::NULL_LITERAL)
                    {
                        // Integer/float/string literals are not boolean predicates
                        if (lit->literalType() == LiteralExpr::INTEGER ||
                            lit->literalType() == LiteralExpr::FLOAT ||
                            lit->literalType() == LiteralExpr::STRING)
                        {
                            reportError(using_expr,
                                "USING clause must evaluate to a boolean expression");
                        }
                    }
                }
                // Column references, comparisons, logical ops are acceptable
            }

            // P2-16: Validate WITH CHECK expression if present
            if (node->hasWithCheckExpr() && node->withCheckExpr())
            {
                Expression *check_expr = node->withCheckExpr();

                // Same validation as USING - must be boolean predicate
                if (check_expr->kind() == ASTKind::LITERAL)
                {
                    auto *lit = static_cast<LiteralExpr *>(check_expr);
                    if (lit->literalType() != LiteralExpr::NULL_LITERAL)
                    {
                        if (lit->literalType() == LiteralExpr::INTEGER ||
                            lit->literalType() == LiteralExpr::FLOAT ||
                            lit->literalType() == LiteralExpr::STRING)
                        {
                            reportError(check_expr,
                                "WITH CHECK clause must evaluate to a boolean expression");
                        }
                    }
                }
            }

            // P2-16: Validate command type is valid (enum values are all valid by definition)
            // Just ensure node is properly formed
            (void)node->command();

            // Note: Role existence validation requires catalog access
            // which is done at execution time in executeCreatePolicy()
        }

        void SemanticAnalyzer::visit(DropPolicyStmt *node)
        {
            // Phase 3.4.3 Enhancement: Add catalog-based semantic validation
            // - Validate table exists via catalog lookup
            // - Validate policy exists (if not IF EXISTS)
            // Currently validation happens at execution time
            (void)node;
        }

        void SemanticAnalyzer::visit(AlterTableRLSStmt *node)
        {
            // Phase 3.4.3 Enhancement: Add catalog-based semantic validation
            // - Validate table exists via catalog lookup
            // Currently validation happens at execution time
            (void)node;
        }

        // Convenience function
        std::unique_ptr<SemanticResult> analyzeAST(Statement *stmt, const StringPool &pool)
        {
            SemanticAnalyzer analyzer(pool);
            return std::make_unique<SemanticResult>(analyzer.analyze(stmt));
        }

    } // namespace parser
} // namespace scratchbird