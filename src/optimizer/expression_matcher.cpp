#include "scratchbird/optimizer/expression_matcher.h"
#include <cstring>

namespace scratchbird::optimizer
{
    using namespace scratchbird::parser;

    // ========================================================================
    // Public Methods
    // ========================================================================

    bool ExpressionMatcher::matches(const Expression *query_expr,
                                     const Expression *index_expr,
                                     const StringPool *string_pool)
    {
        // Null check
        if (!query_expr || !index_expr)
        {
            return false;
        }

        // Must be same expression type
        if (query_expr->kind() != index_expr->kind())
        {
            return false;
        }

        // Dispatch to type-specific matcher
        switch (query_expr->kind())
        {
        case ASTKind::LITERAL:
            return matchLiteral(static_cast<const LiteralExpr *>(query_expr),
                               static_cast<const LiteralExpr *>(index_expr));

        case ASTKind::IDENTIFIER:
            return matchIdentifier(static_cast<const IdentifierExpr *>(query_expr),
                                  static_cast<const IdentifierExpr *>(index_expr),
                                  string_pool);

        case ASTKind::BINARY_OP:
            return matchBinaryOp(static_cast<const BinaryOpExpr *>(query_expr),
                                static_cast<const BinaryOpExpr *>(index_expr),
                                string_pool);

        case ASTKind::FUNCTION_CALL:
            return matchFunctionCall(static_cast<const FunctionCallExpr *>(query_expr),
                                    static_cast<const FunctionCallExpr *>(index_expr),
                                    string_pool);

        case ASTKind::CAST:
            return matchCast(static_cast<const CastExpr *>(query_expr),
                            static_cast<const CastExpr *>(index_expr),
                            string_pool);

        case ASTKind::CASE:
            return matchCase(static_cast<const CaseExpr *>(query_expr),
                            static_cast<const CaseExpr *>(index_expr),
                            string_pool);

        case ASTKind::AGGREGATE_FUNC:
            return matchAggregate(static_cast<const AggregateExpr *>(query_expr),
                                 static_cast<const AggregateExpr *>(index_expr),
                                 string_pool);

        case ASTKind::COALESCE:
            return matchCoalesce(static_cast<const CoalesceExpr *>(query_expr),
                                static_cast<const CoalesceExpr *>(index_expr),
                                string_pool);

        case ASTKind::NULLIF:
            return matchNullIf(static_cast<const NullIfExpr *>(query_expr),
                              static_cast<const NullIfExpr *>(index_expr),
                              string_pool);

        default:
            // Unsupported expression type
            return false;
        }
    }

    ExpressionMatchType ExpressionMatcher::canUse(const Expression *query_expr,
                                                   const Expression *index_expr,
                                                   const StringPool *string_pool)
    {
        // Exact match?
        if (matches(query_expr, index_expr, string_pool))
        {
            return ExpressionMatchType::EXACT_MATCH;
        }

        // Check if query_expr is a comparison involving index_expr
        if (query_expr->kind() == ASTKind::BINARY_OP)
        {
            auto *bin_op = static_cast<const BinaryOpExpr *>(query_expr);
            BinaryOp op = bin_op->op();

            // Check if either left or right matches index expression
            const Expression *left = bin_op->left();
            const Expression *right = bin_op->right();

            if (matches(left, index_expr, string_pool))
            {
                return isOperatorCompatible(op, left, right, index_expr, string_pool);
            }
            else if (matches(right, index_expr, string_pool))
            {
                // Swap operands conceptually and check
                return isOperatorCompatible(op, right, left, index_expr, string_pool);
            }
        }

        return ExpressionMatchType::NO_MATCH;
    }

    ExpressionMatchType ExpressionMatcher::isOperatorCompatible(BinaryOp op,
                                                                const Expression *left_expr,
                                                                const Expression *right_expr,
                                                                const Expression *index_expr,
                                                                const StringPool *string_pool)
    {
        // left_expr matches index_expr, check if operator is index-compatible

        using parser::BinaryOp;

        switch (op)
        {
        // Equality - exact match
        case BinaryOp::EQ:
            return ExpressionMatchType::EXACT_MATCH;

        // Range operators - range scan
        case BinaryOp::LT:
        case BinaryOp::GT:
        case BinaryOp::LE:
        case BinaryOp::GE:
        case BinaryOp::NE:
            return ExpressionMatchType::RANGE_SCAN;

        // LIKE operator - check if prefix scan
        case BinaryOp::LIKE:
        case BinaryOp::ILIKE:
            // Check if right operand is a literal with prefix pattern
            if (right_expr->kind() == ASTKind::LITERAL)
            {
                auto *lit = static_cast<const LiteralExpr *>(right_expr);
                if (isLikePrefixScan(lit))
                {
                    return ExpressionMatchType::RANGE_SCAN;
                }
            }
            return ExpressionMatchType::NO_MATCH;

        // IN operator - exact match (multiple point lookups)
        case BinaryOp::IN:
            return ExpressionMatchType::EXACT_MATCH;

        default:
            return ExpressionMatchType::NO_MATCH;
        }
    }

    // ========================================================================
    // Type-Specific Matchers
    // ========================================================================

    bool ExpressionMatcher::matchLiteral(const LiteralExpr *q, const LiteralExpr *i)
    {
        // Must have same literal type
        if (q->literalType() != i->literalType())
        {
            return false;
        }

        // Compare values based on type
        using LiteralType = parser::LiteralExpr::LiteralType;

        switch (q->literalType())
        {
        case LiteralType::INTEGER:
            return q->intValue() == i->intValue();

        case LiteralType::FLOAT:
            return q->floatValue() == i->floatValue();

        case LiteralType::STRING:
            return q->stringValue() == i->stringValue();

        case LiteralType::NULL_LITERAL:
            return true; // Both NULL

        default:
            return false;
        }
    }

    bool ExpressionMatcher::matchIdentifier(const IdentifierExpr *q,
                                           const IdentifierExpr *i,
                                           const StringPool *pool)
    {
        if (!pool)
        {
            return false;
        }

        // Compare column names (resolve StringIds)
        std::string q_name(pool->get(q->name()));
        std::string i_name(pool->get(i->name()));

        // Case-insensitive comparison (PostgreSQL behavior)
        return strcasecmp(q_name.c_str(), i_name.c_str()) == 0;
    }

    bool ExpressionMatcher::matchBinaryOp(const BinaryOpExpr *q,
                                         const BinaryOpExpr *i,
                                         const StringPool *pool)
    {
        BinaryOp q_op = q->op();
        BinaryOp i_op = i->op();

        // Operators must match
        if (q_op != i_op)
        {
            return false;
        }

        const Expression *q_left = q->left();
        const Expression *q_right = q->right();
        const Expression *i_left = i->left();
        const Expression *i_right = i->right();

        // Try direct match
        if (matches(q_left, i_left, pool) && matches(q_right, i_right, pool))
        {
            return true;
        }

        // If commutative, try swapped
        if (isCommutativeOperator(q_op))
        {
            if (matches(q_left, i_right, pool) && matches(q_right, i_left, pool))
            {
                return true;
            }
        }

        return false;
    }

    bool ExpressionMatcher::matchFunctionCall(const FunctionCallExpr *q,
                                             const FunctionCallExpr *i,
                                             const StringPool *pool)
    {
        if (!pool)
        {
            return false;
        }

        // Function names must match (case-insensitive)
        std::string q_func(pool->get(q->name()));
        std::string i_func(pool->get(i->name()));

        if (strcasecmp(q_func.c_str(), i_func.c_str()) != 0)
        {
            return false;
        }

        // Argument count must match
        const auto &q_args = q->args();
        const auto &i_args = i->args();

        if (q_args.size() != i_args.size())
        {
            return false;
        }

        // All arguments must match
        for (size_t idx = 0; idx < q_args.size(); idx++)
        {
            if (!matches(q_args[idx], i_args[idx], pool))
            {
                return false;
            }
        }

        return true;
    }

    bool ExpressionMatcher::matchCast(const CastExpr *q,
                                     const CastExpr *i,
                                     const StringPool *pool)
    {
        // Target type must match (compare type field of TypeName)
        if (q->targetType().type != i->targetType().type)
        {
            return false;
        }

        // Expression must match
        return matches(q->expr(), i->expr(), pool);
    }

    bool ExpressionMatcher::matchCase(const CaseExpr *q,
                                     const CaseExpr *i,
                                     const StringPool *pool)
    {
        // Number of WHEN clauses must match
        const auto &q_whens = q->whenClauses();
        const auto &i_whens = i->whenClauses();

        if (q_whens.size() != i_whens.size())
        {
            return false;
        }

        // Each WHEN clause must match
        for (size_t idx = 0; idx < q_whens.size(); idx++)
        {
            // Condition must match
            if (!matches(q_whens[idx].condition, i_whens[idx].condition, pool))
            {
                return false;
            }

            // Result must match
            if (!matches(q_whens[idx].result, i_whens[idx].result, pool))
            {
                return false;
            }
        }

        // ELSE clause must match (both present or both absent)
        const Expression *q_else = q->elseResult();
        const Expression *i_else = i->elseResult();

        if ((q_else == nullptr) != (i_else == nullptr))
        {
            return false;
        }

        if (q_else && i_else)
        {
            if (!matches(q_else, i_else, pool))
            {
                return false;
            }
        }

        return true;
    }

    bool ExpressionMatcher::matchAggregate(const AggregateExpr *q,
                                          const AggregateExpr *i,
                                          const StringPool *pool)
    {
        // Aggregate function must match
        if (q->func() != i->func())
        {
            return false;
        }

        // Argument must match (can be nullptr for COUNT(*))
        const Expression *q_arg = q->arg();
        const Expression *i_arg = i->arg();

        if ((q_arg == nullptr) != (i_arg == nullptr))
        {
            return false;
        }

        if (q_arg && i_arg)
        {
            if (!matches(q_arg, i_arg, pool))
            {
                return false;
            }
        }

        // DISTINCT flag must match
        if (q->distinct() != i->distinct())
        {
            return false;
        }

        return true;
    }

    bool ExpressionMatcher::matchCoalesce(const CoalesceExpr *q,
                                         const CoalesceExpr *i,
                                         const StringPool *pool)
    {
        // Number of arguments must match
        const auto &q_args = q->args();
        const auto &i_args = i->args();

        if (q_args.size() != i_args.size())
        {
            return false;
        }

        // All arguments must match in order
        for (size_t idx = 0; idx < q_args.size(); idx++)
        {
            if (!matches(q_args[idx], i_args[idx], pool))
            {
                return false;
            }
        }

        return true;
    }

    bool ExpressionMatcher::matchNullIf(const NullIfExpr *q,
                                       const NullIfExpr *i,
                                       const StringPool *pool)
    {
        // Both expressions must match
        return matches(q->expr1(), i->expr1(), pool) &&
               matches(q->expr2(), i->expr2(), pool);
    }

    // ========================================================================
    // Helper Methods
    // ========================================================================

    bool ExpressionMatcher::isCommutativeOperator(BinaryOp op)
    {
        using parser::BinaryOp;

        switch (op)
        {
        case BinaryOp::ADD:         // a + b == b + a
        case BinaryOp::MULTIPLY:    // a * b == b * a
        case BinaryOp::EQ:          // a = b == b = a
        case BinaryOp::NE:          // a != b == b != a
        case BinaryOp::AND:         // a AND b == b AND a
        case BinaryOp::OR:          // a OR b == b OR a
            return true;

        default:
            return false;
        }
    }

    bool ExpressionMatcher::isComparisonOperator(BinaryOp op)
    {
        using parser::BinaryOp;

        switch (op)
        {
        case BinaryOp::EQ:
        case BinaryOp::NE:
        case BinaryOp::LT:
        case BinaryOp::LE:
        case BinaryOp::GT:
        case BinaryOp::GE:
            return true;

        default:
            return false;
        }
    }

    bool ExpressionMatcher::isLikePrefixScan(const LiteralExpr *pattern)
    {
        using LiteralType = parser::LiteralExpr::LiteralType;

        if (pattern->literalType() != LiteralType::STRING)
        {
            return false;
        }

        // StringId needs to be resolved to get actual string
        // For now, we can't easily check the pattern without a string pool
        // Return false conservatively
        return false;
    }

    ExpressionMatchType ExpressionMatcher::getMatchTypeForOperator(BinaryOp op)
    {
        using parser::BinaryOp;

        switch (op)
        {
        case BinaryOp::EQ:
        case BinaryOp::IN:
            return ExpressionMatchType::EXACT_MATCH;

        case BinaryOp::LT:
        case BinaryOp::LE:
        case BinaryOp::GT:
        case BinaryOp::GE:
        case BinaryOp::NE:
            return ExpressionMatchType::RANGE_SCAN;

        default:
            return ExpressionMatchType::NO_MATCH;
        }
    }

} // namespace scratchbird::optimizer
