#include "scratchbird/sblr/expression_evaluator.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace scratchbird::sblr
{
    // Task 17 MGA Phase 1.4: Updated constructor to include transaction context
    ExpressionEvaluator::ExpressionEvaluator(const std::vector<core::CatalogManager::ColumnInfo> &columns,
                                             parser::StringPool *pool,
                                             core::Database *db,
                                             uint64_t xid)
        : pool_(pool), db_(db), xid_(xid)
    {
        // Build column position map for fast lookups
        for (size_t i = 0; i < columns.size(); i++)
        {
            // Get StringId for column name from pool
            parser::StringPool::StringId col_id = pool_->intern(columns[i].column_name);
            column_positions_[col_id] = i;
        }
    }

    TypedValue ExpressionEvaluator::evaluate(const Expression *expr,
                                              const std::vector<TypedValue> &row)
    {
        if (!expr)
        {
            return TypedValue::makeNull();
        }

        switch (expr->kind())
        {
        case ASTKind::LITERAL:
            return evaluateLiteral(static_cast<const LiteralExpr *>(expr), row);

        case ASTKind::IDENTIFIER:
            return evaluateIdentifier(static_cast<const IdentifierExpr *>(expr), row);

        case ASTKind::BINARY_OP:
            return evaluateBinaryOp(static_cast<const BinaryOpExpr *>(expr), row);

        case ASTKind::FUNCTION_CALL:
            return evaluateFunctionCall(static_cast<const FunctionCallExpr *>(expr), row);

        case ASTKind::CAST:
            return evaluateCast(static_cast<const CastExpr *>(expr), row);

        case ASTKind::CASE:
            return evaluateCase(static_cast<const CaseExpr *>(expr), row);

        case ASTKind::AGGREGATE_FUNC:
            return evaluateAggregate(static_cast<const AggregateExpr *>(expr), row);

        case ASTKind::COALESCE:
            return evaluateCoalesce(static_cast<const CoalesceExpr *>(expr), row);

        case ASTKind::NULLIF:
            return evaluateNullIf(static_cast<const NullIfExpr *>(expr), row);

        case ASTKind::WINDOW_FUNC:
        case ASTKind::JSON_FUNC:
            // Window and JSON functions not supported in expression indexes
            throw std::runtime_error("Window/JSON functions not supported in expression indexes");

        case ASTKind::SUBQUERY:
            throw std::runtime_error("Subqueries not supported in expression indexes");

        default:
            throw std::runtime_error("Unsupported expression type for evaluation");
        }
    }

    bool ExpressionEvaluator::evaluatePredicate(const Expression *predicate,
                                                 const std::vector<TypedValue> &row)
    {
        TypedValue result = evaluate(predicate, row);
        return isTruthy(result);
    }

    // ========================================================================
    // Expression Type Handlers
    // ========================================================================

    TypedValue ExpressionEvaluator::evaluateLiteral(const LiteralExpr *expr,
                                                     const std::vector<TypedValue> &row)
    {
        switch (expr->literalType())
        {
        case parser::LiteralExpr::LiteralType::INTEGER:
        {
            int64_t int_val = expr->intValue();
            return TypedValue::makeInt64(int_val);
        }

        case parser::LiteralExpr::LiteralType::FLOAT:
        {
            double float_val = expr->floatValue();
            return TypedValue::makeFloat64(float_val);
        }

        case parser::LiteralExpr::LiteralType::STRING:
        {
            std::string_view str_val = pool_->get(expr->stringValue());
            return TypedValue::makeVarchar(std::string(str_val));
        }

        case parser::LiteralExpr::LiteralType::NULL_LITERAL:
            return TypedValue::makeNull();

        default:
            throw std::runtime_error("Unsupported literal type");
        }
    }

    TypedValue ExpressionEvaluator::evaluateIdentifier(const IdentifierExpr *expr,
                                                        const std::vector<TypedValue> &row)
    {
        // Look up column in position map
        auto it = column_positions_.find(expr->name());
        if (it == column_positions_.end())
        {
            std::string col_name(pool_->get(expr->name()));
            throw std::runtime_error("Column not found: " + col_name);
        }

        size_t pos = it->second;
        if (pos >= row.size())
        {
            throw std::runtime_error("Row index out of bounds");
        }

        return row[pos];
    }

    TypedValue ExpressionEvaluator::evaluateBinaryOp(const BinaryOpExpr *expr,
                                                      const std::vector<TypedValue> &row)
    {
        TypedValue left = evaluate(expr->left(), row);
        TypedValue right = evaluate(expr->right(), row);

        // Handle NULL propagation for most operators
        if (left.isNull() || right.isNull())
        {
            return TypedValue::makeNull();
        }

        using parser::BinaryOp;

        switch (expr->op())
        {
        // Arithmetic
        case BinaryOp::ADD:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
                return TypedValue::makeInt64(left.getInt64() + right.getInt64());
            else
                return TypedValue::makeFloat64(left.toDouble() + right.toDouble());

        case BinaryOp::SUBTRACT:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
                return TypedValue::makeInt64(left.getInt64() - right.getInt64());
            else
                return TypedValue::makeFloat64(left.toDouble() - right.toDouble());

        case BinaryOp::MULTIPLY:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
                return TypedValue::makeInt64(left.getInt64() * right.getInt64());
            else
                return TypedValue::makeFloat64(left.toDouble() * right.toDouble());

        case BinaryOp::DIVIDE:
            if (right.toDouble() == 0.0)
                throw std::runtime_error("Division by zero");
            return TypedValue::makeFloat64(left.toDouble() / right.toDouble());

        case BinaryOp::MODULO:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
                return TypedValue::makeInt64(left.getInt64() % right.getInt64());
            else
                return TypedValue::makeFloat64(std::fmod(left.toDouble(), right.toDouble()));

        // Comparison
        case BinaryOp::EQ:
            return TypedValue::makeBoolean(compareValues(left, right) == 0);

        case BinaryOp::NE:
            return TypedValue::makeBoolean(compareValues(left, right) != 0);

        case BinaryOp::LT:
            return TypedValue::makeBoolean(compareValues(left, right) < 0);

        case BinaryOp::LE:
            return TypedValue::makeBoolean(compareValues(left, right) <= 0);

        case BinaryOp::GT:
            return TypedValue::makeBoolean(compareValues(left, right) > 0);

        case BinaryOp::GE:
            return TypedValue::makeBoolean(compareValues(left, right) >= 0);

        // Logical
        case BinaryOp::AND:
            return TypedValue::makeBoolean(isTruthy(left) && isTruthy(right));

        case BinaryOp::OR:
            return TypedValue::makeBoolean(isTruthy(left) || isTruthy(right));

        // String
        case BinaryOp::LIKE:
        {
            // Simple LIKE implementation (no wildcards for now)
            std::string left_str = left.toString();
            std::string pattern = right.toString();
            // TODO: Implement proper LIKE with % and _ wildcards
            return TypedValue::makeBoolean(left_str.find(pattern) != std::string::npos);
        }

        default:
            throw std::runtime_error("Unsupported binary operator");
        }
    }

    TypedValue ExpressionEvaluator::evaluateFunctionCall(const FunctionCallExpr *expr,
                                                          const std::vector<TypedValue> &row)
    {
        std::string func_name(pool_->get(expr->name()));
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::toupper);

        const auto &args = expr->args();

        // Evaluate arguments
        std::vector<TypedValue> arg_values;
        for (auto *arg : args)
        {
            arg_values.push_back(evaluate(arg, row));
        }

        // String functions
        if (func_name == "LOWER")
        {
            if (arg_values.empty())
                throw std::runtime_error("LOWER requires 1 argument");
            std::string str = arg_values[0].toString();
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            return TypedValue::makeVarchar(str);
        }
        else if (func_name == "UPPER")
        {
            if (arg_values.empty())
                throw std::runtime_error("UPPER requires 1 argument");
            std::string str = arg_values[0].toString();
            std::transform(str.begin(), str.end(), str.begin(), ::toupper);
            return TypedValue::makeVarchar(str);
        }
        else if (func_name == "LENGTH" || func_name == "LEN")
        {
            if (arg_values.empty())
                throw std::runtime_error("LENGTH requires 1 argument");
            return TypedValue::makeInt64(arg_values[0].toString().length());
        }
        else if (func_name == "ABS")
        {
            if (arg_values.empty())
                throw std::runtime_error("ABS requires 1 argument");
            if (arg_values[0].type() == core::DataType::INT64)
                return TypedValue::makeInt64(std::abs(arg_values[0].getInt64()));
            else
                return TypedValue::makeFloat64(std::abs(arg_values[0].toDouble()));
        }
        else if (func_name == "ROUND")
        {
            if (arg_values.empty())
                throw std::runtime_error("ROUND requires at least 1 argument");
            double value = arg_values[0].toDouble();
            if (arg_values.size() > 1)
            {
                int64_t decimals = arg_values[1].toInt64();
                double multiplier = std::pow(10.0, static_cast<double>(decimals));
                return TypedValue::makeFloat64(std::round(value * multiplier) / multiplier);
            }
            else
            {
                return TypedValue::makeFloat64(std::round(value));
            }
        }

        throw std::runtime_error("Unknown function: " + func_name);
    }

    TypedValue ExpressionEvaluator::evaluateCast(const CastExpr *expr,
                                                  const std::vector<TypedValue> &row)
    {
        TypedValue value = evaluate(expr->expr(), row);
        return castValue(value, expr->targetType().type);
    }

    TypedValue ExpressionEvaluator::evaluateCase(const CaseExpr *expr,
                                                  const std::vector<TypedValue> &row)
    {
        const auto &whens = expr->whenClauses();
        for (const auto &when_clause : whens)
        {
            TypedValue condition = evaluate(when_clause.condition, row);
            if (isTruthy(condition))
            {
                return evaluate(when_clause.result, row);
            }
        }

        // No WHEN matched, return ELSE
        if (expr->elseResult())
        {
            return evaluate(expr->elseResult(), row);
        }

        return TypedValue::makeNull();
    }

    TypedValue ExpressionEvaluator::evaluateAggregate(const AggregateExpr *expr,
                                                       const std::vector<TypedValue> &row)
    {
        // Aggregate functions cannot be evaluated on a single row
        // They require multiple rows and are not supported in expression indexes
        throw std::runtime_error("Aggregate functions not supported in expression indexes");
    }

    TypedValue ExpressionEvaluator::evaluateCoalesce(const CoalesceExpr *expr,
                                                      const std::vector<TypedValue> &row)
    {
        const auto &args = expr->args();
        for (auto *arg : args)
        {
            TypedValue value = evaluate(arg, row);
            if (!value.isNull())
            {
                return value;
            }
        }
        return TypedValue::makeNull();
    }

    TypedValue ExpressionEvaluator::evaluateNullIf(const NullIfExpr *expr,
                                                    const std::vector<TypedValue> &row)
    {
        TypedValue value1 = evaluate(expr->expr1(), row);
        TypedValue value2 = evaluate(expr->expr2(), row);

        if (compareValues(value1, value2) == 0)
        {
            return TypedValue::makeNull();
        }

        return value1;
    }

    // ========================================================================
    // Helper Methods
    // ========================================================================

    TypedValue ExpressionEvaluator::castValue(const TypedValue &value, core::DataType target_type)
    {
        if (value.isNull())
        {
            return TypedValue::makeNull();
        }

        switch (target_type)
        {
        case core::DataType::INT64:
            return TypedValue::makeInt64(value.toInt64());

        case core::DataType::FLOAT64:
            return TypedValue::makeFloat64(value.toDouble());

        case core::DataType::VARCHAR:
        case core::DataType::TEXT:
            return TypedValue::makeVarchar(value.toString());

        case core::DataType::BOOLEAN:
            return TypedValue::makeBoolean(value.toBoolean());

        default:
            throw std::runtime_error("Unsupported cast target type");
        }
    }

    bool ExpressionEvaluator::isTruthy(const TypedValue &value)
    {
        if (value.isNull())
        {
            return false;
        }

        if (value.type() == core::DataType::BOOLEAN)
        {
            return value.getBoolean();
        }

        // Non-NULL, non-boolean values are truthy
        return true;
    }

    int ExpressionEvaluator::compareValues(const TypedValue &left, const TypedValue &right)
    {
        // NULL handling
        if (left.isNull() && right.isNull())
            return 0;
        if (left.isNull())
            return -1; // NULL is less than any value
        if (right.isNull())
            return 1;

        // Type-specific comparison
        if (left.type() == core::DataType::VARCHAR || left.type() == core::DataType::TEXT ||
            right.type() == core::DataType::VARCHAR || right.type() == core::DataType::TEXT)
        {
            std::string left_str = left.toString();
            std::string right_str = right.toString();
            if (left_str < right_str)
                return -1;
            if (left_str > right_str)
                return 1;
            return 0;
        }
        else if (left.type() == core::DataType::BOOLEAN || right.type() == core::DataType::BOOLEAN)
        {
            bool left_bool = left.toBoolean();
            bool right_bool = right.toBoolean();
            if (left_bool < right_bool)
                return -1;
            if (left_bool > right_bool)
                return 1;
            return 0;
        }
        else
        {
            // Numeric comparison
            double left_num = left.toDouble();
            double right_num = right.toDouble();
            if (left_num < right_num)
                return -1;
            if (left_num > right_num)
                return 1;
            return 0;
        }
    }

    // ========================================================================
    // Task 17 MGA Phase 1.4: Transaction-Aware Evaluation
    // ========================================================================

    bool ExpressionEvaluator::evaluateForTuple(const Expression *expr,
                                               const core::TID &tid,
                                               TypedValue &result_out)
    {
        // Task 17 MGA Phase 1.4: Method skeleton with visibility checking
        //
        // NOTE: This method is not yet fully implemented. Full implementation requires:
        // 1. Table ID (to call getTuple correctly)
        // 2. Column information (for deserialization)
        // 3. deserializeTuple implementation
        //
        // The infrastructure is in place (transaction context, visibility checks),
        // but the complete implementation is deferred until needed.

        (void)expr;  // Suppress unused parameter warning
        (void)tid;
        (void)result_out;

        // Check if we have database context
        if (!db_)
        {
            throw std::runtime_error("ExpressionEvaluator::evaluateForTuple requires database context");
        }

        // Future implementation would:
        // 1. Fetch tuple: db_->storage_engine()->getTuple(table_id, tid, &tuple, nullptr)
        // 2. Check visibility: isVisible(hdr->xmin, hdr->xmax, xid_)
        // 3. Deserialize tuple to TypedValue vector
        // 4. Evaluate expression: result_out = evaluate(expr, row_values)
        // 5. Return true on success

        throw std::runtime_error("ExpressionEvaluator::evaluateForTuple not yet fully implemented - requires table context");
    }

    bool ExpressionEvaluator::evaluatePredicateForTuple(const Expression *predicate,
                                                        const core::TID &tid,
                                                        bool &result_out)
    {
        // Task 17 MGA Phase 1.4: Method skeleton with visibility checking
        //
        // NOTE: This method is not yet fully implemented. Full implementation requires:
        // 1. Table ID (to call getTuple correctly)
        // 2. Column information (for deserialization)
        // 3. deserializeTuple implementation
        //
        // The infrastructure is in place (transaction context, visibility checks),
        // but the complete implementation is deferred until needed.

        (void)predicate;  // Suppress unused parameter warning
        (void)tid;
        (void)result_out;

        // Check if we have database context
        if (!db_)
        {
            throw std::runtime_error("ExpressionEvaluator::evaluatePredicateForTuple requires database context");
        }

        // Future implementation would:
        // 1. Fetch tuple: db_->storage_engine()->getTuple(table_id, tid, &tuple, nullptr)
        // 2. Check visibility: isVisible(hdr->xmin, hdr->xmax, xid_)
        // 3. Deserialize tuple to TypedValue vector
        // 4. Evaluate predicate: result_out = evaluatePredicate(predicate, row_values)
        // 5. Return true on success

        throw std::runtime_error("ExpressionEvaluator::evaluatePredicateForTuple not yet fully implemented - requires table context");
    }

} // namespace scratchbird::sblr
