/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/predicate_matcher.h"
#include "scratchbird/optimizer/expression_matcher.h"
#include <algorithm>
#include <limits>

namespace scratchbird::optimizer
{
    // ========================================================================
    // Public Methods
    // ========================================================================

    bool PredicateMatcher::implies(const Expression *query_pred,
                                    const Expression *index_pred)
    {
        if (!query_pred || !index_pred)
        {
            return false;
        }

        if (predicatesEqual(query_pred, index_pred))
        {
            return true;
        }

        if (containsConjunct(query_pred, index_pred))
        {
            return true;
        }

        if (query_pred->kind() == ExprKind::BINARY_OP &&
            index_pred->kind() == ExprKind::BINARY_OP)
        {
            auto *query_binop = static_cast<const BinaryOpExpr *>(query_pred);
            auto *index_binop = static_cast<const BinaryOpExpr *>(index_pred);

            if (rangeImplies(query_binop, index_binop))
            {
                return true;
            }
        }

        if (impliesNotNull(query_pred, index_pred))
        {
            return true;
        }

        return false;
    }

    bool PredicateMatcher::containsConjunct(const Expression *query_pred,
                                            const Expression *index_pred)
    {
        if (!query_pred || !index_pred)
        {
            return false;
        }

        std::vector<const Expression *> conjuncts;
        collectConjuncts(query_pred, conjuncts);

        for (const Expression *conjunct : conjuncts)
        {
            if (predicatesEqual(conjunct, index_pred))
            {
                return true;
            }
        }

        return false;
    }

    // ========================================================================
    // Private Methods
    // ========================================================================

    bool PredicateMatcher::predicatesEqual(const Expression *pred1,
                                           const Expression *pred2)
    {
        return ExpressionMatcher::matches(pred1, pred2);
    }

    bool PredicateMatcher::rangeImplies(const BinaryOpExpr *query_binop,
                                        const BinaryOpExpr *index_binop)
    {
        BinaryOp query_op = query_binop->op();
        BinaryOp index_op = index_binop->op();

        const Expression *query_left = query_binop->left();
        const Expression *query_right = query_binop->right();
        const Expression *index_left = index_binop->left();
        const Expression *index_right = index_binop->right();

        const IdentifierExpr *query_col = extractColumn(query_left);
        const IdentifierExpr *index_col = extractColumn(index_left);

        if (!query_col || !index_col)
        {
            return false;
        }

        if (!ExpressionMatcher::matches(query_col, index_col))
        {
            return false;
        }

        if (query_right->kind() != ExprKind::LITERAL ||
            index_right->kind() != ExprKind::LITERAL)
        {
            return false;
        }

        const LiteralExpr *query_lit = static_cast<const LiteralExpr *>(query_right);
        const LiteralExpr *index_lit = static_cast<const LiteralExpr *>(index_right);

        int cmp = compareLiterals(query_lit, index_lit);
        if (cmp == std::numeric_limits<int>::min())
        {
            return false;
        }

        if (query_op == BinaryOp::GT && index_op == BinaryOp::GT)
        {
            return cmp >= 0;
        }
        else if (query_op == BinaryOp::GE && index_op == BinaryOp::GT)
        {
            return cmp > 0;
        }
        else if (query_op == BinaryOp::GE && index_op == BinaryOp::GE)
        {
            return cmp >= 0;
        }
        else if (query_op == BinaryOp::LT && index_op == BinaryOp::LT)
        {
            return cmp <= 0;
        }
        else if (query_op == BinaryOp::LE && index_op == BinaryOp::LT)
        {
            return cmp < 0;
        }
        else if (query_op == BinaryOp::LE && index_op == BinaryOp::LE)
        {
            return cmp <= 0;
        }
        else if (query_op == BinaryOp::EQ && index_op == BinaryOp::EQ)
        {
            return cmp == 0;
        }
        else if (query_op == BinaryOp::EQ && index_op == BinaryOp::GT)
        {
            return cmp > 0;
        }
        else if (query_op == BinaryOp::EQ && index_op == BinaryOp::GE)
        {
            return cmp >= 0;
        }
        else if (query_op == BinaryOp::EQ && index_op == BinaryOp::LT)
        {
            return cmp < 0;
        }
        else if (query_op == BinaryOp::EQ && index_op == BinaryOp::LE)
        {
            return cmp <= 0;
        }

        return false;
    }

    bool PredicateMatcher::impliesNotNull(const Expression *query_pred,
                                          const Expression *index_not_null)
    {
        if (!query_pred || !index_not_null)
        {
            return false;
        }

        if (query_pred->kind() == ExprKind::BINARY_OP)
        {
            auto *query_binop = static_cast<const BinaryOpExpr *>(query_pred);
            BinaryOp op = query_binop->op();

            if (!operatorImpliesNotNull(op))
            {
                return false;
            }

            const IdentifierExpr *query_col = extractColumn(query_binop->left());
            if (!query_col)
            {
                query_col = extractColumn(query_binop->right());
            }

            if (!query_col)
            {
                return false;
            }

            (void)index_not_null;
            return false;
        }

        return false;
    }

    const IdentifierExpr *PredicateMatcher::extractColumn(const Expression *expr)
    {
        if (!expr)
        {
            return nullptr;
        }

        if (expr->kind() == ExprKind::IDENTIFIER)
        {
            return static_cast<const IdentifierExpr *>(expr);
        }

        return nullptr;
    }

    int PredicateMatcher::compareLiterals(const LiteralExpr *lit1, const LiteralExpr *lit2)
    {
        if (!lit1 || !lit2)
        {
            return std::numeric_limits<int>::min();
        }

        if (lit1->literalType() != lit2->literalType())
        {
            return std::numeric_limits<int>::min();
        }

        switch (lit1->literalType())
        {
        case LiteralExpr::LiteralType::INTEGER:
        {
            int64_t val1 = lit1->intValue();
            int64_t val2 = lit2->intValue();
            if (val1 < val2) return -1;
            if (val1 > val2) return 1;
            return 0;
        }

        case LiteralExpr::LiteralType::FLOAT:
        {
            double val1 = lit1->floatValue();
            double val2 = lit2->floatValue();
            if (val1 < val2) return -1;
            if (val1 > val2) return 1;
            return 0;
        }

        case LiteralExpr::LiteralType::STRING:
            return lit1->stringValue().compare(lit2->stringValue());

        case LiteralExpr::LiteralType::NULL_LITERAL:
        case LiteralExpr::LiteralType::RANGE:
            return std::numeric_limits<int>::min();

        default:
            return std::numeric_limits<int>::min();
        }
    }

    bool PredicateMatcher::operatorImpliesNotNull(BinaryOp op)
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

    void PredicateMatcher::collectConjuncts(const Expression *expr,
                                            std::vector<const Expression *> &conjuncts)
    {
        if (!expr)
        {
            return;
        }

        if (expr->kind() == ExprKind::BINARY_OP)
        {
            auto *binop = static_cast<const BinaryOpExpr *>(expr);
            if (binop->op() == BinaryOp::AND)
            {
                collectConjuncts(binop->left(), conjuncts);
                collectConjuncts(binop->right(), conjuncts);
                return;
            }
        }

        conjuncts.push_back(expr);
    }

    std::string PredicateMatcher::normalizeIdentifier(std::string_view name)
    {
        std::string normalized(name);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
        return normalized;
    }

} // namespace scratchbird::optimizer
