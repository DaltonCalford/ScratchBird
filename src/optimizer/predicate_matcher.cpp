#include "scratchbird/optimizer/predicate_matcher.h"
#include "scratchbird/optimizer/expression_matcher.h"
#include <cstring>
#include <limits>

namespace scratchbird::optimizer
{
    using namespace scratchbird::parser;

    // ========================================================================
    // Public Methods
    // ========================================================================

    bool PredicateMatcher::implies(const Expression *query_pred,
                                    const Expression *index_pred,
                                    const StringPool *query_pool,
                                    const StringPool *index_pool)
    {
        // If index_pool not provided, use query_pool for both
        if (!index_pool)
        {
            index_pool = query_pool;
        }

        // Null check
        if (!query_pred || !index_pred || !query_pool)
        {
            return false;
        }

        // Case 1: Exact match (query_pred == index_pred)
        if (predicatesEqual(query_pred, index_pred, query_pool, index_pool))
        {
            return true;
        }

        // Case 2: Query contains index predicate as conjunct
        // Example: (a = 1 AND b = 2) implies (b = 2)
        if (containsConjunct(query_pred, index_pred, query_pool, index_pool))
        {
            return true;
        }

        // Case 3: Range implication
        // Example: age > 30 implies age > 18
        if (query_pred->kind() == ASTKind::BINARY_OP &&
            index_pred->kind() == ASTKind::BINARY_OP)
        {
            auto *query_binop = static_cast<const BinaryOpExpr *>(query_pred);
            auto *index_binop = static_cast<const BinaryOpExpr *>(index_pred);

            if (rangeImplies(query_binop, index_binop, query_pool, index_pool))
            {
                return true;
            }
        }

        // Case 4: NOT NULL implication
        // Example: "col = 5" implies "col IS NOT NULL"
        if (impliesNotNull(query_pred, index_pred, query_pool, index_pool))
        {
            return true;
        }

        return false;
    }

    bool PredicateMatcher::containsConjunct(const Expression *query_pred,
                                            const Expression *index_pred,
                                            const StringPool *query_pool,
                                            const StringPool *index_pool)
    {
        // If index_pool not provided, use query_pool for both
        if (!index_pool)
        {
            index_pool = query_pool;
        }

        if (!query_pred || !index_pred || !query_pool)
        {
            return false;
        }

        // Collect all AND conjuncts from query
        std::vector<const Expression *> conjuncts;
        collectConjuncts(query_pred, conjuncts);

        // Check if any conjunct matches index predicate
        for (const Expression *conjunct : conjuncts)
        {
            if (predicatesEqual(conjunct, index_pred, query_pool, index_pool))
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
                                           const Expression *pred2,
                                           const StringPool *query_pool,
                                           const StringPool *index_pool)
    {
        // Use ExpressionMatcher for structural equality
        return ExpressionMatcher::matches(pred1, pred2, query_pool, index_pool);
    }

    bool PredicateMatcher::rangeImplies(const BinaryOpExpr *query_binop,
                                        const BinaryOpExpr *index_binop,
                                        const StringPool *query_pool,
                                        const StringPool *index_pool)
    {
        // Both must be comparison operators
        using parser::BinaryOp;

        BinaryOp query_op = query_binop->op();
        BinaryOp index_op = index_binop->op();

        // Get left and right operands
        const Expression *query_left = query_binop->left();
        const Expression *query_right = query_binop->right();
        const Expression *index_left = index_binop->left();
        const Expression *index_right = index_binop->right();

        // Check if comparing same column
        // Pattern: col OP literal
        const IdentifierExpr *query_col = extractColumn(query_left);
        const IdentifierExpr *index_col = extractColumn(index_left);

        if (!query_col || !index_col)
        {
            return false;
        }

        // Columns must match
        if (!ExpressionMatcher::matches(query_col, index_col, query_pool, index_pool))
        {
            return false;
        }

        // Right operands must be literals
        if (query_right->kind() != ASTKind::LITERAL ||
            index_right->kind() != ASTKind::LITERAL)
        {
            return false;
        }

        const LiteralExpr *query_lit = static_cast<const LiteralExpr *>(query_right);
        const LiteralExpr *index_lit = static_cast<const LiteralExpr *>(index_right);

        int cmp = compareLiterals(query_lit, index_lit, query_pool, index_pool);
        if (cmp == std::numeric_limits<int>::min())
        {
            // Incomparable literals
            return false;
        }

        // Check implication based on operators
        // Query: col > 30, Index: col > 18 → implies (30 > 18)
        // Query: col < 10, Index: col < 20 → implies (10 < 20)

        if (query_op == BinaryOp::GT && index_op == BinaryOp::GT)
        {
            // col > Q implies col > I if Q >= I
            return cmp >= 0; // query_lit >= index_lit
        }
        else if (query_op == BinaryOp::GE && index_op == BinaryOp::GT)
        {
            // col >= Q implies col > I if Q > I
            return cmp > 0; // query_lit > index_lit
        }
        else if (query_op == BinaryOp::GE && index_op == BinaryOp::GE)
        {
            // col >= Q implies col >= I if Q >= I
            return cmp >= 0;
        }
        else if (query_op == BinaryOp::LT && index_op == BinaryOp::LT)
        {
            // col < Q implies col < I if Q <= I
            return cmp <= 0; // query_lit <= index_lit
        }
        else if (query_op == BinaryOp::LE && index_op == BinaryOp::LT)
        {
            // col <= Q implies col < I if Q < I
            return cmp < 0; // query_lit < index_lit
        }
        else if (query_op == BinaryOp::LE && index_op == BinaryOp::LE)
        {
            // col <= Q implies col <= I if Q <= I
            return cmp <= 0;
        }
        else if ((query_op == BinaryOp::EQ || query_op == BinaryOp::EQ) &&
                 (index_op == BinaryOp::EQ || index_op == BinaryOp::EQ))
        {
            // col = Q implies col = I if Q == I
            return cmp == 0;
        }
        else if ((query_op == BinaryOp::EQ || query_op == BinaryOp::EQ) &&
                 index_op == BinaryOp::GT)
        {
            // col = Q implies col > I if Q > I
            return cmp > 0;
        }
        else if ((query_op == BinaryOp::EQ || query_op == BinaryOp::EQ) &&
                 index_op == BinaryOp::GE)
        {
            // col = Q implies col >= I if Q >= I
            return cmp >= 0;
        }
        else if ((query_op == BinaryOp::EQ || query_op == BinaryOp::EQ) &&
                 index_op == BinaryOp::LT)
        {
            // col = Q implies col < I if Q < I
            return cmp < 0;
        }
        else if ((query_op == BinaryOp::EQ || query_op == BinaryOp::EQ) &&
                 index_op == BinaryOp::LE)
        {
            // col = Q implies col <= I if Q <= I
            return cmp <= 0;
        }

        return false;
    }

    bool PredicateMatcher::impliesNotNull(const Expression *query_pred,
                                          const Expression *index_not_null,
                                          const StringPool *query_pool,
                                          const StringPool *index_pool)
    {
        // Check if index predicate is "col IS NOT NULL"
        // In AST, this might be represented as a UnaryOp or specific node type
        // For now, we'll handle the common case where query_pred uses a column
        // with an operator that implies NOT NULL

        if (!query_pred || !index_not_null || !query_pool)
        {
            return false;
        }

        // If query is a comparison (=, <, >, <=, >=), it implies NOT NULL
        if (query_pred->kind() == ASTKind::BINARY_OP)
        {
            auto *query_binop = static_cast<const BinaryOpExpr *>(query_pred);
            BinaryOp op = query_binop->op();

            if (!operatorImpliesNotNull(op))
            {
                return false;
            }

            // Extract column from query predicate
            const IdentifierExpr *query_col = extractColumn(query_binop->left());
            if (!query_col)
            {
                query_col = extractColumn(query_binop->right());
            }

            if (!query_col)
            {
                return false;
            }

            // Check if index predicate is IS NOT NULL on same column
            // This is a simplified check - full implementation would need
            // proper IS NOT NULL AST node handling
            // For now, return false as this requires deeper AST inspection
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

        if (expr->kind() == ASTKind::IDENTIFIER)
        {
            return static_cast<const IdentifierExpr *>(expr);
        }

        // Could extend to handle CAST, etc.
        return nullptr;
    }

    int PredicateMatcher::compareLiterals(const LiteralExpr *lit1, const LiteralExpr *lit2,
                                           const StringPool *pool1, const StringPool *pool2)
    {
        if (!lit1 || !lit2)
        {
            return std::numeric_limits<int>::min();
        }

        // Must be same type
        if (lit1->literalType() != lit2->literalType())
        {
            return std::numeric_limits<int>::min();
        }

        switch (lit1->literalType())
        {
        case parser::LiteralExpr::LiteralType::INTEGER:
        {
            int64_t val1 = lit1->intValue();
            int64_t val2 = lit2->intValue();
            if (val1 < val2) return -1;
            if (val1 > val2) return 1;
            return 0;
        }

        case parser::LiteralExpr::LiteralType::FLOAT:
        {
            double val1 = lit1->floatValue();
            double val2 = lit2->floatValue();
            if (val1 < val2) return -1;
            if (val1 > val2) return 1;
            return 0;
        }

        case parser::LiteralExpr::LiteralType::STRING:
        {
            // Resolve StringIds using the respective StringPools
            if (pool1 && pool2)
            {
                std::string str1(pool1->get(lit1->stringValue()));
                std::string str2(pool2->get(lit2->stringValue()));
                return str1.compare(str2);
            }
            // Fallback: compare StringIds directly (only works if same pool)
            StringPool::StringId val1 = lit1->stringValue();
            StringPool::StringId val2 = lit2->stringValue();
            if (val1 < val2) return -1;
            if (val1 > val2) return 1;
            return 0;
        }

        case parser::LiteralExpr::LiteralType::NULL_LITERAL:
            // NULL is not comparable
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

        // If this is an AND binary op, recursively collect both sides
        if (expr->kind() == ASTKind::BINARY_OP)
        {
            auto *binop = static_cast<const BinaryOpExpr *>(expr);
            if (binop->op() == BinaryOp::AND)
            {
                collectConjuncts(binop->left(), conjuncts);
                collectConjuncts(binop->right(), conjuncts);
                return;
            }
        }

        // Otherwise, this is a leaf conjunct
        conjuncts.push_back(expr);
    }

} // namespace scratchbird::optimizer
