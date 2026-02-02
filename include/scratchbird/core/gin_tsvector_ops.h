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
#include "scratchbird/core/gin_index.h"
#include <vector>
#include <cstdint>

namespace scratchbird::core
{

/**
 * @brief GIN Operator Class for TSVector
 *
 * This class provides the operator class interface for indexing tsvector
 * values in a GIN (Generalized Inverted Index). It extracts lexemes from
 * tsvector values for indexing and evaluates tsquery matches for lookups.
 *
 * PostgreSQL Compatibility:
 * - Matches PostgreSQL's gin_tsvector_ops operator class
 * - Supports @@ operator for text search matching
 * - Each unique lexeme becomes a key in the GIN index
 * - Posting lists store TIDs of documents containing each lexeme
 */
class GINTSVectorOps
{
public:
    /**
     * @brief Extract indexable keys from a tsvector value
     *
     * For a tsvector, each unique lexeme becomes a key. Position and weight
     * information is discarded for the index (but preserved in the original
     * tsvector column).
     *
     * Example:
     *   Input: 'cat':1A,3B 'dog':2 'bird':4
     *   Output: ["cat", "dog", "bird"]
     *
     * @param tsvector_data Serialized tsvector data
     * @param tsvector_len Length of serialized data
     * @return Vector of keys (each key is a byte vector)
     */
    static auto extractKeys(const void* tsvector_data, size_t tsvector_len)
        -> std::vector<std::vector<uint8_t>>;

    /**
     * @brief Extract indexable keys from a TSVector object
     *
     * Convenience overload that works directly with TSVector objects.
     *
     * @param tsvector The TSVector to extract keys from
     * @return Vector of keys (each key is a byte vector)
     */
    static auto extractKeys(const TSVector& tsvector)
        -> std::vector<std::vector<uint8_t>>;

    /**
     * @brief Extract search keys from a tsquery for index lookup
     *
     * For a tsquery, we extract all lexeme terms (not operators). The GIN
     * index is then queried for documents containing these lexemes, and the
     * full Boolean evaluation is done on the results.
     *
     * Example:
     *   Input: (cat & dog) | bird
     *   Output: ["cat", "dog", "bird"]
     *
     * @param tsquery_data Serialized tsquery data
     * @param tsquery_len Length of serialized data
     * @return Vector of search keys (each key is a byte vector)
     */
    static auto extractQueryKeys(const void* tsquery_data, size_t tsquery_len)
        -> std::vector<std::vector<uint8_t>>;

    /**
     * @brief Extract search keys from a TSQuery object
     *
     * Convenience overload that works directly with TSQuery objects.
     *
     * @param tsquery The TSQuery to extract keys from
     * @return Vector of search keys (each key is a byte vector)
     */
    static auto extractQueryKeys(const TSQuery& tsquery)
        -> std::vector<std::vector<uint8_t>>;

    /**
     * @brief Check if a tsvector value is consistent with a tsquery
     *
     * This is the "consistent" function in PostgreSQL GIN terminology. It's
     * called after GIN has retrieved candidate documents (those containing
     * at least some of the query lexemes) and needs to evaluate the full
     * Boolean expression.
     *
     * @param tsvector_data Serialized tsvector data
     * @param tsvector_len Length of serialized data
     * @param tsquery_data Serialized tsquery data
     * @param tsquery_len Length of serialized data
     * @return true if tsvector matches tsquery, false otherwise
     */
    static auto consistent(const void* tsvector_data, size_t tsvector_len,
                          const void* tsquery_data, size_t tsquery_len) -> bool;

    /**
     * @brief Check if a TSVector matches a TSQuery
     *
     * Convenience overload that works directly with TSVector and TSQuery objects.
     *
     * @param tsvector The document vector
     * @param tsquery The search query
     * @return true if tsvector matches tsquery, false otherwise
     */
    static auto consistent(const TSVector& tsvector, const TSQuery& tsquery) -> bool;

    /**
     * @brief Determine query strategy for GIN lookup
     *
     * Analyzes the query structure to determine the optimal GIN lookup strategy:
     * - NEED_ALL: All keys must be present (for AND queries)
     * - NEED_ANY: Any key can be present (for OR queries)
     * - NEED_RECHECK: Complex query requiring recheck
     *
     * @param tsquery The search query
     * @return Query strategy enum
     */
    enum class QueryStrategy
    {
        NEED_ALL,    // All keys must be present (AND query)
        NEED_ANY,    // Any key can be present (OR query)
        NEED_RECHECK // Complex query, need to recheck all candidates
    };

    static auto analyzeQuery(const TSQuery& tsquery) -> QueryStrategy;

    /**
     * @brief Estimate selectivity of a query
     *
     * Returns an estimate (0.0 to 1.0) of what fraction of the index
     * will match the query. Used by the query planner for cost estimation.
     *
     * @param tsquery The search query
     * @param index_stats Statistics from the GIN index
     * @return Selectivity estimate (0.0 = no matches, 1.0 = all match)
     */
    static auto estimateSelectivity(const TSQuery& tsquery,
                                    const GinIndex::Statistics& index_stats) -> double;
};

/**
 * @brief Helper function to create a key extractor for TSVector values
 *
 * Returns a lambda that can be passed to GinIndex::insert() for extracting
 * keys from tsvector values.
 *
 * Example:
 *   auto extractor = makeGINTSVectorKeyExtractor();
 *   gin_index->insert(tsvector_data, tsvector_len, tid, extractor);
 */
inline auto makeGINTSVectorKeyExtractor()
{
    return [](const void* data, size_t len) -> std::vector<std::vector<uint8_t>> {
        return GINTSVectorOps::extractKeys(data, len);
    };
}

} // namespace scratchbird::core
