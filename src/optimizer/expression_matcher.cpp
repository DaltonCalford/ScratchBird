#include "scratchbird/optimizer/expression_matcher.h"
#include <algorithm>
#include <cctype>

namespace scratchbird::optimizer
{
    // ========================================================================
    // Public Methods
    // ========================================================================

    bool ExpressionMatcher::matches(const Expression *query_expr,
                                     const Expression *index_expr)
    {
        if (!query_expr || !index_expr)
        {
            return false;
        }

        if (query_expr->kind() != index_expr->kind())
        {
            return false;
        }

        switch (query_expr->kind())
        {
        case ExprKind::LITERAL:
            return matchLiteral(static_cast<const LiteralExpr *>(query_expr),
                               static_cast<const LiteralExpr *>(index_expr));

        case ExprKind::IDENTIFIER:
            return matchIdentifier(static_cast<const IdentifierExpr *>(query_expr),
                                  static_cast<const IdentifierExpr *>(index_expr));

        case ExprKind::BINARY_OP:
            return matchBinaryOp(static_cast<const BinaryOpExpr *>(query_expr),
                                static_cast<const BinaryOpExpr *>(index_expr));

        case ExprKind::FUNCTION_CALL:
            return matchFunctionCall(static_cast<const FunctionCallExpr *>(query_expr),
                                    static_cast<const FunctionCallExpr *>(index_expr));

        case ExprKind::CAST:
            return matchCast(static_cast<const CastExpr *>(query_expr),
                            static_cast<const CastExpr *>(index_expr));

        case ExprKind::CASE:
            return matchCase(static_cast<const CaseExpr *>(query_expr),
                            static_cast<const CaseExpr *>(index_expr));

        case ExprKind::AGGREGATE:
            return matchAggregate(static_cast<const AggregateExpr *>(query_expr),
                                 static_cast<const AggregateExpr *>(index_expr));

        case ExprKind::COALESCE:
            return matchCoalesce(static_cast<const CoalesceExpr *>(query_expr),
                                static_cast<const CoalesceExpr *>(index_expr));

        case ExprKind::NULLIF:
            return matchNullIf(static_cast<const NullIfExpr *>(query_expr),
                              static_cast<const NullIfExpr *>(index_expr));

        case ExprKind::EXTRACT:
            return matchExtract(static_cast<const ExtractExpr *>(query_expr),
                               static_cast<const ExtractExpr *>(index_expr));

        default:
            return false;
        }
    }

    ExpressionMatchType ExpressionMatcher::canUse(const Expression *query_expr,
                                                   const Expression *index_expr)
    {
        if (matches(query_expr, index_expr))
        {
            return ExpressionMatchType::EXACT_MATCH;
        }

        if (query_expr && query_expr->kind() == ExprKind::BINARY_OP)
        {
            auto *bin_op = static_cast<const BinaryOpExpr *>(query_expr);
            BinaryOp op = bin_op->op();

            const Expression *left = bin_op->left();
            const Expression *right = bin_op->right();

            if (matches(left, index_expr))
            {
                return isOperatorCompatible(op, left, right, index_expr);
            }
            else if (matches(right, index_expr))
            {
                return isOperatorCompatible(op, right, left, index_expr);
            }
        }

        return ExpressionMatchType::NO_MATCH;
    }

    ExpressionMatchType ExpressionMatcher::isOperatorCompatible(BinaryOp op,
                                                                const Expression *left_expr,
                                                                const Expression *right_expr,
                                                                const Expression *index_expr)
    {
        (void)left_expr;
        (void)index_expr;

        switch (op)
        {
        case BinaryOp::EQ:
            return ExpressionMatchType::EXACT_MATCH;

        case BinaryOp::LT:
        case BinaryOp::GT:
        case BinaryOp::LE:
        case BinaryOp::GE:
        case BinaryOp::NE:
            return ExpressionMatchType::RANGE_SCAN;

        case BinaryOp::LIKE:
        case BinaryOp::ILIKE:
            if (right_expr && right_expr->kind() == ExprKind::LITERAL)
            {
                auto *lit = static_cast<const LiteralExpr *>(right_expr);
                if (isLikePrefixScan(lit))
                {
                    return ExpressionMatchType::RANGE_SCAN;
                }
            }
            return ExpressionMatchType::NO_MATCH;

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
        if (q->literalType() != i->literalType())
        {
            return false;
        }

        switch (q->literalType())
        {
        case LiteralExpr::LiteralType::INTEGER:
            return q->intValue() == i->intValue();

        case LiteralExpr::LiteralType::FLOAT:
            return q->floatValue() == i->floatValue();

        case LiteralExpr::LiteralType::STRING:
            return q->stringValue() == i->stringValue();

        case LiteralExpr::LiteralType::NULL_LITERAL:
            return true;

        case LiteralExpr::LiteralType::RANGE:
            return q->rangeValue() == i->rangeValue();

        default:
            return false;
        }
    }

    bool ExpressionMatcher::matchIdentifier(const IdentifierExpr *q,
                                           const IdentifierExpr *i)
    {
        return normalizeIdentifier(q->name()) == normalizeIdentifier(i->name());
    }

    bool ExpressionMatcher::matchBinaryOp(const BinaryOpExpr *q,
                                         const BinaryOpExpr *i)
    {
        BinaryOp q_op = q->op();
        BinaryOp i_op = i->op();

        if (q_op != i_op)
        {
            return false;
        }

        const Expression *q_left = q->left();
        const Expression *q_right = q->right();
        const Expression *i_left = i->left();
        const Expression *i_right = i->right();

        if (matches(q_left, i_left) && matches(q_right, i_right))
        {
            return true;
        }

        if (isCommutativeOperator(q_op))
        {
            if (matches(q_left, i_right) && matches(q_right, i_left))
            {
                return true;
            }
        }

        return false;
    }

    bool ExpressionMatcher::matchFunctionCall(const FunctionCallExpr *q,
                                             const FunctionCallExpr *i)
    {
        if (normalizeFunctionName(q->name()) != normalizeFunctionName(i->name()))
        {
            return false;
        }

        const auto &q_args = q->args();
        const auto &i_args = i->args();

        if (q_args.size() != i_args.size())
        {
            return false;
        }

        for (size_t idx = 0; idx < q_args.size(); idx++)
        {
            if (!matches(q_args[idx].get(), i_args[idx].get()))
            {
                return false;
            }
        }

        return true;
    }

    bool ExpressionMatcher::matchCast(const CastExpr *q,
                                     const CastExpr *i)
    {
        const auto& q_type = q->targetType();
        const auto& i_type = i->targetType();
        if (q_type.type != i_type.type ||
            q_type.precision != i_type.precision ||
            q_type.scale != i_type.scale ||
            q_type.element_type != i_type.element_type ||
            q_type.with_timezone != i_type.with_timezone ||
            q_type.timezone_hint != i_type.timezone_hint ||
            q->format() != i->format())
        {
            return false;
        }

        return matches(q->expr(), i->expr());
    }

    bool ExpressionMatcher::matchCase(const CaseExpr *q,
                                     const CaseExpr *i)
    {
        const auto &q_whens = q->whenClauses();
        const auto &i_whens = i->whenClauses();

        if (q_whens.size() != i_whens.size())
        {
            return false;
        }

        for (size_t idx = 0; idx < q_whens.size(); idx++)
        {
            if (!matches(q_whens[idx].condition.get(), i_whens[idx].condition.get()))
            {
                return false;
            }

            if (!matches(q_whens[idx].result.get(), i_whens[idx].result.get()))
            {
                return false;
            }
        }

        const Expression *q_else = q->elseResult();
        const Expression *i_else = i->elseResult();

        if ((q_else == nullptr) != (i_else == nullptr))
        {
            return false;
        }

        if (q_else && i_else)
        {
            if (!matches(q_else, i_else))
            {
                return false;
            }
        }

        return true;
    }

    bool ExpressionMatcher::matchAggregate(const AggregateExpr *q,
                                          const AggregateExpr *i)
    {
        if (q->func() != i->func())
        {
            return false;
        }

        const Expression *q_arg = q->arg();
        const Expression *i_arg = i->arg();

        if ((q_arg == nullptr) != (i_arg == nullptr))
        {
            return false;
        }

        if (q_arg && i_arg)
        {
            if (!matches(q_arg, i_arg))
            {
                return false;
            }
        }

        if (q->distinct() != i->distinct())
        {
            return false;
        }

        return true;
    }

    bool ExpressionMatcher::matchCoalesce(const CoalesceExpr *q,
                                         const CoalesceExpr *i)
    {
        const auto &q_args = q->args();
        const auto &i_args = i->args();

        if (q_args.size() != i_args.size())
        {
            return false;
        }

        for (size_t idx = 0; idx < q_args.size(); idx++)
        {
            if (!matches(q_args[idx].get(), i_args[idx].get()))
            {
                return false;
            }
        }

        return true;
    }

    bool ExpressionMatcher::matchNullIf(const NullIfExpr *q,
                                       const NullIfExpr *i)
    {
        return matches(q->expr1(), i->expr1()) &&
               matches(q->expr2(), i->expr2());
    }

    bool ExpressionMatcher::matchExtract(const ExtractExpr *q,
                                        const ExtractExpr *i)
    {
        if (q->fieldId() != i->fieldId())
        {
            return false;
        }

        return matches(q->source(), i->source());
    }

    // ========================================================================
    // Helper Methods
    // ========================================================================

    bool ExpressionMatcher::isCommutativeOperator(BinaryOp op)
    {
        switch (op)
        {
        case BinaryOp::ADD:
        case BinaryOp::MULTIPLY:
        case BinaryOp::EQ:
        case BinaryOp::NE:
        case BinaryOp::AND:
        case BinaryOp::OR:
            return true;

        default:
            return false;
        }
    }

    bool ExpressionMatcher::isComparisonOperator(BinaryOp op)
    {
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
        if (pattern->literalType() != LiteralExpr::LiteralType::STRING)
        {
            return false;
        }

        std::string_view pattern_str = pattern->stringValue();
        if (pattern_str.empty())
        {
            return false;
        }

        if (pattern_str[0] == '%' || pattern_str[0] == '_')
        {
            return false;
        }

        size_t wildcard_pos = pattern_str.find('%');
        if (wildcard_pos == std::string_view::npos)
        {
            return true;
        }

        return wildcard_pos > 0;
    }

    std::string ExpressionMatcher::normalizeIdentifier(std::string_view name)
    {
        std::string normalized(name);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
        return normalized;
    }

    std::string ExpressionMatcher::normalizeFunctionName(std::string_view name)
    {
        std::string normalized(name);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
        return normalized;
    }

    ExpressionMatchType ExpressionMatcher::getMatchTypeForOperator(BinaryOp op)
    {
        switch (op)
        {
        case BinaryOp::EQ:
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
