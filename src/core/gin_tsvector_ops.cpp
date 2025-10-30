#include "scratchbird/core/gin_tsvector_ops.h"
#include <unordered_set>
#include <cstring>
#include <functional>
#include <cmath>
#include <algorithm>

namespace scratchbird::core
{

// ============================================================================
// Key Extraction from TSVector
// ============================================================================

auto GINTSVectorOps::extractKeys(const void* tsvector_data, size_t tsvector_len)
    -> std::vector<std::vector<uint8_t>>
{
    // Deserialize the tsvector
    auto tsvector = TSVector::fromBinary(
        static_cast<const uint8_t*>(tsvector_data), tsvector_len
    );

    if (!tsvector.has_value())
    {
        // Invalid tsvector, return empty keys
        return {};
    }

    return extractKeys(*tsvector);
}

auto GINTSVectorOps::extractKeys(const TSVector& tsvector)
    -> std::vector<std::vector<uint8_t>>
{
    std::vector<std::vector<uint8_t>> keys;
    keys.reserve(tsvector.numLexemes());

    // Each unique lexeme becomes a key
    const auto& lexemes = tsvector.lexemes();
    for (const auto& lexeme : lexemes)
    {
        // Convert lexeme word to byte vector
        const std::string& word = lexeme.word;
        std::vector<uint8_t> key(word.begin(), word.end());
        keys.push_back(std::move(key));
    }

    return keys;
}

// ============================================================================
// Query Key Extraction from TSQuery
// ============================================================================

/**
 * Helper: Recursively extract all lexeme terms from a TSQuery tree
 */
static void extractQueryTerms(const TSQueryNode* node,
                              std::unordered_set<std::string>& terms)
{
    if (!node)
        return;

    if (node->type() == TSQueryNode::Type::LEXEME)
    {
        terms.insert(node->term());
    }
    else
    {
        // Recursively extract from children
        extractQueryTerms(node->left(), terms);
        extractQueryTerms(node->right(), terms);
    }
}

auto GINTSVectorOps::extractQueryKeys(const void* tsquery_data, size_t tsquery_len)
    -> std::vector<std::vector<uint8_t>>
{
    // Deserialize the tsquery
    auto tsquery = TSQuery::fromBinary(
        static_cast<const uint8_t*>(tsquery_data), tsquery_len
    );

    if (!tsquery.has_value())
    {
        // Invalid tsquery, return empty keys
        return {};
    }

    return extractQueryKeys(*tsquery);
}

auto GINTSVectorOps::extractQueryKeys(const TSQuery& tsquery)
    -> std::vector<std::vector<uint8_t>>
{
    // Extract all unique lexeme terms from the query
    std::unordered_set<std::string> terms;
    extractQueryTerms(tsquery.root(), terms);

    // Convert to byte vectors
    std::vector<std::vector<uint8_t>> keys;
    keys.reserve(terms.size());

    for (const auto& term : terms)
    {
        std::vector<uint8_t> key(term.begin(), term.end());
        keys.push_back(std::move(key));
    }

    return keys;
}

// ============================================================================
// Consistent Function (Match Evaluation)
// ============================================================================

auto GINTSVectorOps::consistent(const void* tsvector_data, size_t tsvector_len,
                                const void* tsquery_data, size_t tsquery_len) -> bool
{
    // Deserialize both tsvector and tsquery
    auto tsvector = TSVector::fromBinary(
        static_cast<const uint8_t*>(tsvector_data), tsvector_len
    );

    auto tsquery = TSQuery::fromBinary(
        static_cast<const uint8_t*>(tsquery_data), tsquery_len
    );

    if (!tsvector.has_value() || !tsquery.has_value())
    {
        return false;
    }

    return consistent(*tsvector, *tsquery);
}

auto GINTSVectorOps::consistent(const TSVector& tsvector, const TSQuery& tsquery) -> bool
{
    // Delegate to the TSQuery::matches method from Phase 1
    return tsquery.matches(tsvector);
}

// ============================================================================
// Query Analysis
// ============================================================================

/**
 * Helper: Analyze query structure to determine strategy
 */
static auto analyzeQueryNode(const TSQueryNode* node)
    -> GINTSVectorOps::QueryStrategy
{
    if (!node)
        return GINTSVectorOps::QueryStrategy::NEED_RECHECK;

    switch (node->type())
    {
    case TSQueryNode::Type::LEXEME:
        // Single term - need this one key
        return GINTSVectorOps::QueryStrategy::NEED_ALL;

    case TSQueryNode::Type::AND:
        {
            // AND: need all keys from both sides
            auto left_strategy = analyzeQueryNode(node->left());
            auto right_strategy = analyzeQueryNode(node->right());

            // If either side is ANY or RECHECK, we need recheck
            if (left_strategy != GINTSVectorOps::QueryStrategy::NEED_ALL ||
                right_strategy != GINTSVectorOps::QueryStrategy::NEED_ALL)
            {
                return GINTSVectorOps::QueryStrategy::NEED_RECHECK;
            }

            return GINTSVectorOps::QueryStrategy::NEED_ALL;
        }

    case TSQueryNode::Type::OR:
        // OR: need any key from either side
        return GINTSVectorOps::QueryStrategy::NEED_ANY;

    case TSQueryNode::Type::NOT:
        // NOT: complex, need recheck
        return GINTSVectorOps::QueryStrategy::NEED_RECHECK;

    case TSQueryNode::Type::PHRASE:
        // Phrase: complex, need recheck (position-sensitive)
        return GINTSVectorOps::QueryStrategy::NEED_RECHECK;

    default:
        return GINTSVectorOps::QueryStrategy::NEED_RECHECK;
    }
}

auto GINTSVectorOps::analyzeQuery(const TSQuery& tsquery) -> QueryStrategy
{
    return analyzeQueryNode(tsquery.root());
}

// ============================================================================
// Selectivity Estimation
// ============================================================================

/**
 * Helper: Count lexemes in query
 */
static auto countQueryLexemes(const TSQueryNode* node) -> size_t
{
    if (!node)
        return 0;

    if (node->type() == TSQueryNode::Type::LEXEME)
        return 1;

    return countQueryLexemes(node->left()) + countQueryLexemes(node->right());
}

/**
 * Helper: Check if query is pure AND
 */
static auto isPureAND(const TSQueryNode* node) -> bool
{
    if (!node)
        return true;

    if (node->type() == TSQueryNode::Type::LEXEME)
        return true;

    if (node->type() == TSQueryNode::Type::AND)
    {
        return isPureAND(node->left()) && isPureAND(node->right());
    }

    return false;
}

/**
 * Helper: Check if query is pure OR
 */
static auto isPureOR(const TSQueryNode* node) -> bool
{
    if (!node)
        return true;

    if (node->type() == TSQueryNode::Type::LEXEME)
        return true;

    if (node->type() == TSQueryNode::Type::OR)
    {
        return isPureOR(node->left()) && isPureOR(node->right());
    }

    return false;
}

auto GINTSVectorOps::estimateSelectivity(const TSQuery& tsquery,
                                         const GinIndex::Statistics& index_stats) -> double
{
    if (!tsquery.root())
        return 0.0;

    // Count lexemes in query
    size_t query_lexemes = countQueryLexemes(tsquery.root());
    if (query_lexemes == 0)
        return 0.0;

    // Get average TIDs per key from index stats
    double avg_tids_per_key = index_stats.avg_tids_per_key;
    if (avg_tids_per_key <= 0.0 || index_stats.num_tuples == 0)
    {
        // No stats available, use conservative estimate
        return 0.1;
    }

    // Calculate individual key selectivity
    double key_selectivity = avg_tids_per_key / index_stats.num_tuples;

    // Analyze query structure
    if (query_lexemes == 1)
    {
        // Single term: just return key selectivity
        return std::min(key_selectivity, 1.0);
    }
    else if (isPureAND(tsquery.root()))
    {
        // AND: multiply selectivities (intersection gets smaller)
        // Use geometric mean to avoid underestimating too much
        double selectivity = std::pow(key_selectivity, query_lexemes * 0.5);
        return std::min(selectivity, 1.0);
    }
    else if (isPureOR(tsquery.root()))
    {
        // OR: use inclusion-exclusion principle
        // P(A ∪ B) ≈ P(A) + P(B) - P(A)P(B) for two events
        // For multiple events, approximate as: 1 - (1-p)^n
        // where p is individual key selectivity and n is number of keys
        double prob_not_match = std::pow(1.0 - key_selectivity, query_lexemes);
        double selectivity = 1.0 - prob_not_match;
        return std::min(selectivity, 1.0);
    }
    else
    {
        // Complex query (NOT, PHRASE, mixed): use conservative middle estimate
        // Assume it's somewhat selective but not as much as pure AND
        double selectivity = std::pow(key_selectivity, query_lexemes * 0.25);
        return std::min(selectivity, 0.5);
    }
}

} // namespace scratchbird::core
