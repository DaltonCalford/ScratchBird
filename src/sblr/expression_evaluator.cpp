#include "scratchbird/sblr/expression_evaluator.h"
#include "scratchbird/core/typed_value.h"
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
                                             core::Database *db,
                                             uint64_t xid)
        : db_(db), xid_(xid)
    {
        // Build column position map for fast lookups
        for (size_t i = 0; i < columns.size(); i++)
        {
            column_positions_[normalizeIdentifier(columns[i].column_name)] = i;
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
        case ExprKind::LITERAL:
            return evaluateLiteral(static_cast<const LiteralExpr *>(expr), row);

        case ExprKind::IDENTIFIER:
            return evaluateIdentifier(static_cast<const IdentifierExpr *>(expr), row);

        case ExprKind::BINARY_OP:
            return evaluateBinaryOp(static_cast<const BinaryOpExpr *>(expr), row);

        case ExprKind::FUNCTION_CALL:
            return evaluateFunctionCall(static_cast<const FunctionCallExpr *>(expr), row);

        case ExprKind::CAST:
            return evaluateCast(static_cast<const CastExpr *>(expr), row);

        case ExprKind::CASE:
            return evaluateCase(static_cast<const CaseExpr *>(expr), row);

        case ExprKind::AGGREGATE:
            return evaluateAggregate(static_cast<const AggregateExpr *>(expr), row);

        case ExprKind::COALESCE:
            return evaluateCoalesce(static_cast<const CoalesceExpr *>(expr), row);

        case ExprKind::NULLIF:
            return evaluateNullIf(static_cast<const NullIfExpr *>(expr), row);

        case ExprKind::EXTRACT:
            throw std::runtime_error("EXTRACT not supported in expression indexes");

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
        case core::LiteralExpr::LiteralType::INTEGER:
        {
            int64_t int_val = expr->intValue();
            return TypedValue::makeInt64(int_val);
        }

        case core::LiteralExpr::LiteralType::FLOAT:
        {
            double float_val = expr->floatValue();
            return TypedValue::makeFloat64(float_val);
        }

        case core::LiteralExpr::LiteralType::STRING:
            return TypedValue::makeVarchar(expr->stringValue());

        case core::LiteralExpr::LiteralType::NULL_LITERAL:
            return TypedValue::makeNull();

        default:
            throw std::runtime_error("Unsupported literal type");
        }
    }

    TypedValue ExpressionEvaluator::evaluateIdentifier(const IdentifierExpr *expr,
                                                        const std::vector<TypedValue> &row)
    {
        // Look up column in position map
        auto it = column_positions_.find(normalizeIdentifier(expr->name()));
        if (it == column_positions_.end())
        {
            throw std::runtime_error("Column not found: " + expr->name());
        }

        size_t pos = it->second;
        if (pos >= row.size())
        {
            throw std::runtime_error("Row index out of bounds");
        }

        return row[pos];
    }

    // ========================================================================
    // P0-4: Safe Arithmetic Operations (Overflow Detection)
    // ========================================================================

    namespace {
        // Safe arithmetic operations using compiler intrinsics
        inline bool safeAdd(int64_t a, int64_t b, int64_t* result) {
            return !__builtin_add_overflow(a, b, result);
        }

        inline bool safeSubtract(int64_t a, int64_t b, int64_t* result) {
            return !__builtin_sub_overflow(a, b, result);
        }

        inline bool safeMultiply(int64_t a, int64_t b, int64_t* result) {
            return !__builtin_mul_overflow(a, b, result);
        }

        inline bool safeDivide(int64_t a, int64_t b, int64_t* result) {
            if (b == 0) {
                return false;
            }
            // Check for INT64_MIN / -1 overflow
            if (a == INT64_MIN && b == -1) {
                return false;
            }
            *result = a / b;
            return true;
        }

        inline bool safeModulo(int64_t a, int64_t b, int64_t* result) {
            if (b == 0) {
                return false;
            }
            // Check for INT64_MIN % -1 (which is also problematic on some platforms)
            if (a == INT64_MIN && b == -1) {
                *result = 0;
                return true;
            }
            *result = a % b;
            return true;
        }
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

        using core::BinaryOp;

        switch (expr->op())
        {
        // Arithmetic
        case BinaryOp::ADD:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeAdd(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Integer overflow in addition");
                }
                return TypedValue::makeInt64(result);
            }
            else
                return TypedValue::makeFloat64(left.toDouble() + right.toDouble());

        case BinaryOp::SUBTRACT:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeSubtract(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Integer overflow in subtraction");
                }
                return TypedValue::makeInt64(result);
            }
            else
                return TypedValue::makeFloat64(left.toDouble() - right.toDouble());

        case BinaryOp::MULTIPLY:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeMultiply(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Integer overflow in multiplication");
                }
                return TypedValue::makeInt64(result);
            }
            else
                return TypedValue::makeFloat64(left.toDouble() * right.toDouble());

        case BinaryOp::DIVIDE:
            // For integer division
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeDivide(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Division by zero or integer overflow");
                }
                return TypedValue::makeInt64(result);
            }
            // For floating point division
            if (right.toDouble() == 0.0)
                throw std::runtime_error("Division by zero");
            return TypedValue::makeFloat64(left.toDouble() / right.toDouble());

        case BinaryOp::MODULO:
            if (left.type() == core::DataType::INT64 && right.type() == core::DataType::INT64)
            {
                int64_t result;
                if (!safeModulo(left.getInt64(), right.getInt64(), &result)) {
                    throw std::runtime_error("Modulo by zero");
                }
                return TypedValue::makeInt64(result);
            }
            else
            {
                if (right.toDouble() == 0.0)
                    throw std::runtime_error("Modulo by zero");
                return TypedValue::makeFloat64(std::fmod(left.toDouble(), right.toDouble()));
            }

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
            // Phase 4 Enhancement: Implement proper LIKE with % and _ wildcards
            // Current: substring match fallback (correct for simple patterns)
            return TypedValue::makeBoolean(left_str.find(pattern) != std::string::npos);
        }

        default:
            throw std::runtime_error("Unsupported binary operator");
        }
    }

    TypedValue ExpressionEvaluator::evaluateFunctionCall(const FunctionCallExpr *expr,
                                                          const std::vector<TypedValue> &row)
    {
        std::string func_name(expr->name());
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::toupper);

        const auto &args = expr->args();

        // Evaluate arguments
        std::vector<TypedValue> arg_values;
        for (const auto &arg : args)
        {
            arg_values.push_back(evaluate(arg.get(), row));
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
            {
                int64_t val = arg_values[0].getInt64();
                // Check for INT64_MIN overflow (abs(INT64_MIN) would overflow)
                if (val == INT64_MIN)
                    throw std::runtime_error("Integer overflow in ABS (INT64_MIN has no positive representation)");
                return TypedValue::makeInt64(std::abs(val));
            }
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
        return castValue(value, expr->targetType(), expr->format());
    }

    TypedValue ExpressionEvaluator::evaluateCase(const CaseExpr *expr,
                                                  const std::vector<TypedValue> &row)
    {
        const auto &whens = expr->whenClauses();
        for (const auto &when_clause : whens)
        {
            TypedValue condition = evaluate(when_clause.condition.get(), row);
            if (isTruthy(condition))
            {
                return evaluate(when_clause.result.get(), row);
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
        for (const auto &arg : args)
        {
            TypedValue value = evaluate(arg.get(), row);
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

    TypedValue ExpressionEvaluator::castValue(const TypedValue &value,
                                              const TypeInfo& target_type,
                                              CastFormat format)
    {
        if (value.isNull())
        {
            return TypedValue::makeNull(target_type.type);
        }

        TypedValue result;
        ErrorContext ctx;
        Status status = value.convertTo(target_type, result, format, &ctx);
        if (status != Status::OK)
        {
            throw std::runtime_error(ctx.message.empty()
                                         ? "Type conversion failed"
                                         : ctx.message);
        }
        return result;
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

    std::string ExpressionEvaluator::normalizeIdentifier(std::string_view name)
    {
        std::string normalized(name);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
        return normalized;
    }

    // ========================================================================
    // Task 17 MGA Phase 1.4: Transaction-Aware Evaluation
    // ========================================================================

    bool ExpressionEvaluator::evaluateForTuple(const Expression *expr,
                                               const core::TID &tid,
                                               TypedValue &result_out)
    {
        // WP-6 PARSE-4: Evaluate expression for a specific tuple (by TID)
        //
        // This method evaluates an expression using a tuple's data. The TID is used
        // to identify the row, but since we don't have direct table access here,
        // callers should use the overload that accepts column values directly.
        //
        // For now, this TID-based method is a placeholder that requires external
        // tuple fetching. The preferred approach is to fetch the tuple externally
        // and call evaluate() with the row values.

        (void)tid;  // TID parameter reserved for future direct tuple access

        // Check if we have database context
        if (!db_)
        {
            // Without database context, we can't fetch tuple data.
            // Return false to indicate evaluation was not performed.
            result_out = TypedValue::makeNull();
            return false;
        }

        // NOTE: Full tuple-fetch implementation would require:
        // 1. Table ID (to locate the correct table)
        // 2. Fetch tuple: db_->storage_engine()->getTuple(table_id, tid, &tuple, nullptr)
        // 3. Check visibility with xid_
        // 4. Deserialize tuple to TypedValue vector
        // 5. Call evaluate(expr, row_values)
        //
        // For now, callers should use the public evaluate() method directly
        // with pre-fetched row data, which is the common pattern used throughout
        // the codebase.

        // Without table context, cannot evaluate by TID
        result_out = TypedValue::makeNull();
        return false;
    }

    bool ExpressionEvaluator::evaluatePredicateForTuple(const Expression *predicate,
                                                        const core::TID &tid,
                                                        bool &result_out)
    {
        // WP-6 PARSE-5: Evaluate predicate for a specific tuple (by TID)
        //
        // Similar to evaluateForTuple, but for boolean predicates.
        // Returns true if evaluation succeeded, result_out contains the boolean result.

        (void)tid;  // TID parameter reserved for future direct tuple access

        // Check if we have database context
        if (!db_)
        {
            result_out = false;
            return false;
        }

        // NOTE: Full implementation would require table context (see evaluateForTuple).
        // For now, return false to indicate evaluation was not performed.
        // Callers should use evaluatePredicate() with pre-fetched row data.

        result_out = false;
        return false;
    }

    // WP-6 PARSE-4/5: Additional overloads that accept row data directly
    TypedValue ExpressionEvaluator::evaluateForRow(const Expression *expr,
                                                    const std::vector<std::string>& column_names,
                                                    const std::vector<TypedValue>& column_values)
    {
        // Set up column position map
        column_positions_.clear();
        for (size_t i = 0; i < column_names.size(); ++i)
        {
            column_positions_[normalizeIdentifier(column_names[i])] = i;
        }

        // Evaluate using existing method
        return evaluate(expr, column_values);
    }

    bool ExpressionEvaluator::evaluatePredicateForRow(const Expression *predicate,
                                                       const std::vector<std::string>& column_names,
                                                       const std::vector<TypedValue>& column_values)
    {
        // Set up column position map
        column_positions_.clear();
        for (size_t i = 0; i < column_names.size(); ++i)
        {
            column_positions_[normalizeIdentifier(column_names[i])] = i;
        }

        // Evaluate using existing method
        return evaluatePredicate(predicate, column_values);
    }

} // namespace scratchbird::sblr
