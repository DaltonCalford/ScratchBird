/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include <optional>
#include <string>

namespace scratchbird::core
{

// ============================================================================
// Text Search Match Operator (@@)
// ============================================================================

/**
 * @brief Text search match operator: tsvector @@ tsquery
 *
 * Evaluates whether a document (tsvector) matches a query (tsquery).
 * This is the primary full-text search operator.
 *
 * @param document The document vector to search
 * @param query The search query
 * @return true if document matches query, false otherwise
 *
 * Example:
 *   auto doc = TSVector::fromString("'cat':1 'dog':2");
 *   auto q = TSQuery::fromString("cat & dog");
 *   bool matches = ts_match(*doc, *q);  // returns true
 */
auto ts_match(const TSVector& document, const TSQuery& query) -> bool;

/**
 * @brief Text search match operator: text @@ tsquery
 *
 * Convenience operator that converts text to tsvector using default config
 * and then evaluates the match.
 *
 * @param text The text to search
 * @param query The search query
 * @return true if text matches query after conversion, false otherwise
 *
 * Example:
 *   auto q = TSQuery::fromString("cat & dog");
 *   bool matches = ts_match_text("I have a cat and a dog", *q);
 */
auto ts_match_text(const std::string& text, const TSQuery& query) -> bool;

/**
 * @brief Text search match operator with configuration: text @@ tsquery
 *
 * Converts text to tsvector using specified configuration and evaluates match.
 *
 * @param config_name The text search configuration name ("simple", "english", etc.)
 * @param text The text to search
 * @param query The search query
 * @return true if text matches query after conversion, false otherwise
 */
auto ts_match_text(const std::string& config_name, const std::string& text,
                  const TSQuery& query) -> bool;

// ============================================================================
// Text Search Ranking (TS_RANK)
// ============================================================================

/**
 * @brief Calculate relevance rank for tsvector @@ tsquery match
 *
 * Implements TF-IDF style ranking with position-based weighting.
 * Higher scores indicate better relevance.
 *
 * Ranking formula:
 * - Base score: number of matching lexemes / total lexemes in document
 * - Weighted by lexeme frequencies (term frequency component)
 * - Weighted by position weights (A=1.0, B=0.4, C=0.2, D=0.1)
 * - Normalized by document length (optional)
 *
 * @param document The document vector
 * @param query The search query
 * @param normalization Normalization mode (0=none, 1=divide by length)
 * @return Relevance score (0.0 to 1.0), or 0.0 if no match
 *
 * Example:
 *   auto doc = to_tsvector("english", "the quick brown fox");
 *   auto q = to_tsquery("english", "quick & fox");
 *   double rank = ts_rank(*doc, *q);  // returns ~0.5
 */
auto ts_rank(const TSVector& document, const TSQuery& query,
            int normalization = 0) -> double;

/**
 * @brief Calculate rank with weights for different positions
 *
 * Allows custom weighting for position classes A, B, C, D.
 * Default weights: D=0.1, C=0.2, B=0.4, A=1.0
 *
 * @param weights Array of 4 weights [D, C, B, A]
 * @param document The document vector
 * @param query The search query
 * @param normalization Normalization mode
 * @return Relevance score
 */
auto ts_rank_weighted(const float weights[4], const TSVector& document,
                     const TSQuery& query, int normalization = 0) -> double;

/**
 * @brief Calculate cover density ranking
 *
 * Alternative ranking algorithm based on how tightly query terms appear
 * in the document. Better for phrase-heavy queries.
 *
 * @param document The document vector
 * @param query The search query
 * @param normalization Normalization mode
 * @return Relevance score
 */
auto ts_rank_cd(const TSVector& document, const TSQuery& query,
               int normalization = 0) -> double;

} // namespace scratchbird::core
