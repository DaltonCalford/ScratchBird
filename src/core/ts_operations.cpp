#include "scratchbird/core/ts_operations.h"
#include "scratchbird/core/ts_functions.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <functional>
#include <unordered_map>

namespace scratchbird::core
{

// ============================================================================
// Text Search Match Operator (@@)
// ============================================================================

auto ts_match(const TSVector& document, const TSQuery& query) -> bool
{
    // Delegate to existing TSQuery::matches method
    return query.matches(document);
}

auto ts_match_text(const std::string& text, const TSQuery& query) -> bool
{
    // Convert text to tsvector using default ("simple") configuration
    auto vec = to_tsvector(text);
    if (!vec.has_value())
        return false;

    return query.matches(*vec);
}

auto ts_match_text(const std::string& config_name, const std::string& text,
                  const TSQuery& query) -> bool
{
    // Convert text to tsvector using specified configuration
    auto vec = to_tsvector(config_name, text);
    if (!vec.has_value())
        return false;

    return query.matches(*vec);
}

// ============================================================================
// Text Search Ranking Implementation
// ============================================================================

/**
 * Helper: Count matching lexemes in document
 */
static auto countMatches(const TSVector& document, const TSQuery& query) -> size_t
{
    size_t count = 0;

    // Get all lexemes from query
    std::unordered_set<std::string> query_terms;

    // Helper to extract all lexeme terms from query tree
    std::function<void(const TSQueryNode*)> extractTerms;
    extractTerms = [&](const TSQueryNode* node) {
        if (!node) return;

        if (node->type() == TSQueryNode::Type::LEXEME)
        {
            query_terms.insert(node->term());
        }
        else
        {
            extractTerms(node->left());
            extractTerms(node->right());
        }
    };

    if (query.root())
    {
        extractTerms(query.root());
    }

    // Count how many lexemes in document match query terms
    const auto& doc_lexemes = document.lexemes();
    for (const auto& lex : doc_lexemes)
    {
        if (query_terms.find(lex.word) != query_terms.end())
        {
            count++;
        }
    }

    return count;
}

/**
 * Helper: Calculate weighted score for a lexeme based on position weights
 */
static auto calculateLexemeScore(const Lexeme& lex, const float weights[4]) -> double
{
    if (lex.positions.empty())
        return 0.0;

    double score = 0.0;

    for (size_t i = 0; i < lex.positions.size(); i++)
    {
        // Get weight for this position
        char weight_char = lex.getWeight(i);

        // Map weight character to weight array index: D=0, C=1, B=2, A=3
        float weight;
        switch (weight_char)
        {
        case 'A': weight = weights[3]; break; // A = index 3 = 1.0
        case 'B': weight = weights[2]; break; // B = index 2 = 0.4
        case 'C': weight = weights[1]; break; // C = index 1 = 0.2
        case 'D': weight = weights[0]; break; // D = index 0 = 0.1
        default: weight = weights[0]; break;  // Default to D
        }

        score += weight;
    }

    // Average weight across all positions of this lexeme
    return score / lex.positions.size();
}

auto ts_rank(const TSVector& document, const TSQuery& query, int normalization) -> double
{
    // Default weights: D=0.1, C=0.2, B=0.4, A=1.0
    static const float default_weights[4] = {0.1f, 0.2f, 0.4f, 1.0f};
    return ts_rank_weighted(default_weights, document, query, normalization);
}

auto ts_rank_weighted(const float weights[4], const TSVector& document,
                     const TSQuery& query, int normalization) -> double
{
    // First check if document matches query
    if (!query.matches(document))
        return 0.0;

    // Get matching lexemes
    size_t match_count = countMatches(document, query);
    if (match_count == 0)
        return 0.0;

    // Extract query terms
    std::unordered_set<std::string> query_terms;
    std::function<void(const TSQueryNode*)> extractTerms;
    extractTerms = [&](const TSQueryNode* node) {
        if (!node) return;

        if (node->type() == TSQueryNode::Type::LEXEME)
        {
            query_terms.insert(node->term());
        }
        else
        {
            extractTerms(node->left());
            extractTerms(node->right());
        }
    };

    if (query.root())
    {
        extractTerms(query.root());
    }

    // Calculate weighted score
    double total_score = 0.0;
    size_t scored_count = 0;

    const auto& doc_lexemes = document.lexemes();
    for (const auto& lex : doc_lexemes)
    {
        // Only score lexemes that match query terms
        if (query_terms.find(lex.word) != query_terms.end())
        {
            double lex_score = calculateLexemeScore(lex, weights);

            // Apply term frequency weighting
            // More occurrences = higher weight
            double tf_weight = std::log(1.0 + lex.positions.size());
            total_score += lex_score * tf_weight;
            scored_count++;
        }
    }

    if (scored_count == 0)
        return 0.0;

    // Average score per matching lexeme
    double avg_score = total_score / scored_count;

    // Apply normalization
    if (normalization > 0)
    {
        // Normalization mode 1: divide by document length
        size_t doc_length = document.numLexemes();
        if (doc_length > 0)
        {
            avg_score /= (1.0 + std::log(1.0 + doc_length));
        }
    }

    return avg_score;
}

/**
 * Helper: Find minimum cover of query terms in document
 *
 * A cover is the smallest span of positions that includes all query terms.
 * Cover density is inversely related to the size of this span.
 */
static auto findMinCover(const TSVector& document,
                        const std::unordered_set<std::string>& query_terms) -> double
{
    if (query_terms.empty())
        return 0.0;

    // Collect all positions for each query term in the document
    std::unordered_map<std::string, std::vector<uint16_t>> term_positions;

    const auto& doc_lexemes = document.lexemes();
    for (const auto& lex : doc_lexemes)
    {
        if (query_terms.find(lex.word) != query_terms.end())
        {
            // Extract actual positions (remove weight bits)
            std::vector<uint16_t> positions;
            for (uint16_t pos : lex.positions)
            {
                positions.push_back(pos & 0x3FFF); // Keep only position bits
            }
            term_positions[lex.word] = positions;
        }
    }

    // Check if all query terms appear in document
    if (term_positions.size() != query_terms.size())
        return 0.0;

    // Find minimum span that covers all terms
    // For simplicity, use the difference between min and max positions
    uint16_t min_pos = UINT16_MAX;
    uint16_t max_pos = 0;

    for (const auto& [term, positions] : term_positions)
    {
        for (uint16_t pos : positions)
        {
            min_pos = std::min(min_pos, pos);
            max_pos = std::max(max_pos, pos);
        }
    }

    if (min_pos >= max_pos)
        return 1.0; // All terms at same position

    // Cover density = 1 / (span size)
    // Smaller span = higher density = better match
    double span = static_cast<double>(max_pos - min_pos + 1);
    double density = query_terms.size() / span;

    return density;
}

auto ts_rank_cd(const TSVector& document, const TSQuery& query,
               int normalization) -> double
{
    // First check if document matches query
    if (!query.matches(document))
        return 0.0;

    // Extract query terms
    std::unordered_set<std::string> query_terms;
    std::function<void(const TSQueryNode*)> extractTerms;
    extractTerms = [&](const TSQueryNode* node) {
        if (!node) return;

        if (node->type() == TSQueryNode::Type::LEXEME)
        {
            query_terms.insert(node->term());
        }
        else
        {
            extractTerms(node->left());
            extractTerms(node->right());
        }
    };

    if (query.root())
    {
        extractTerms(query.root());
    }

    if (query_terms.empty())
        return 0.0;

    // Calculate cover density
    double density = findMinCover(document, query_terms);

    // Apply normalization
    if (normalization > 0)
    {
        size_t doc_length = document.numLexemes();
        if (doc_length > 0)
        {
            density /= (1.0 + std::log(1.0 + doc_length));
        }
    }

    return density;
}

} // namespace scratchbird::core
