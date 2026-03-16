/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/index_family_lowering.h"

namespace scratchbird::optimizer
{
    namespace
    {
        using IndexType = core::CatalogManager::IndexType;

        auto makeResult(PlannerAccessFamily family,
                        AccessPathExactnessClass exactness,
                        AccessPathVisibilityEnforcement visibility,
                        bool requires_recheck,
                        bool supports_ordering,
                        bool supports_covering,
                        bool supports_parameterization,
                        AccessPathQueryabilityState queryability_state =
                            AccessPathQueryabilityState::QUERYABLE)
            -> PlannerFamilyLoweringResult
        {
            PlannerFamilyLoweringResult result;
            result.family = family;
            result.family_name = plannerAccessFamilyName(family);
            result.path_name = result.family_name;
            result.exactness_class = exactness;
            result.visibility_enforcement = visibility;
            result.requires_recheck = requires_recheck;
            result.supports_ordering = supports_ordering;
            result.supports_covering = supports_covering;
            result.supports_parameterization = supports_parameterization;
            result.queryability_state = queryability_state;
            return result;
        }

        auto isBtreeLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::BTREE:
                case IndexType::ART:
                case IndexType::STL_SORT:
                case IndexType::MONGODB_GEO_HAYSTACK:
                case IndexType::NEO4J_RANGE:
                case IndexType::NEO4J_POINT:
                case IndexType::REDIS_LIST:
                case IndexType::REDIS_ZSET:
                case IndexType::REDIS_STREAM:
                    return true;
                default:
                    return false;
            }
        }

        auto isHashLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::HASH:
                case IndexType::REDIS_STRING:
                case IndexType::REDIS_HASH:
                case IndexType::REDIS_SET:
                case IndexType::REDIS_HLL:
                    return true;
                default:
                    return false;
            }
        }

        auto isSummaryLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::BRIN:
                    return true;
                case IndexType::BLOOM:
                case IndexType::ZONEMAP:
                    return false;
                default:
                    return false;
            }
        }

        auto isSummaryFilterLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::BLOOM:
                case IndexType::ZONEMAP:
                    return true;
                default:
                    return false;
            }
        }

        auto isGeneralizedLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::GIST:
                case IndexType::SPGIST:
                case IndexType::RTREE:
                case IndexType::MONGODB_2D:
                case IndexType::MONGODB_2DSPHERE:
                case IndexType::MONGODB_2DSPHERE_BUCKET:
                case IndexType::REDIS_GEO:
                    return true;
                default:
                    return false;
            }
        }

        auto isTextLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::FULLTEXT:
                case IndexType::INVERTED:
                case IndexType::MINHASH_LSH:
                case IndexType::SPARSE_INVERTED:
                case IndexType::SPARSE_WAND:
                case IndexType::TRIE:
                case IndexType::NGRAM:
                case IndexType::MONGODB_WILDCARD:
                case IndexType::MONGODB_ENCRYPTED_RANGE:
                case IndexType::NEO4J_TEXT:
                case IndexType::CASSANDRA_SASI:
                case IndexType::CASSANDRA_SAI:
                    return true;
                default:
                    return false;
            }
        }

        auto isVectorFlatLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::VECTOR_FLAT:
                case IndexType::VECTOR_BIN_FLAT:
                    return true;
                default:
                    return false;
            }
        }

        auto isHnswLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::HNSW:
                case IndexType::RHNSW_PQ:
                case IndexType::RHNSW_SQ:
                case IndexType::ANNOY:
                case IndexType::NSG:
                case IndexType::DISKANN:
                case IndexType::SCANN:
                case IndexType::GPU_CAGRA:
                case IndexType::NEO4J_VECTOR:
                    return true;
                default:
                    return false;
            }
        }

        auto isIvfLike(IndexType index_type) -> bool
        {
            switch (index_type)
            {
                case IndexType::IVF:
                case IndexType::IVF_FLAT:
                case IndexType::BIN_IVF_FLAT:
                case IndexType::IVF_PQ:
                case IndexType::IVF_SQ8:
                case IndexType::IVF_SQ8_HYBRID:
                    return true;
                default:
                    return false;
            }
        }
    } // namespace

    auto lowerSequentialPlannerFamily() -> PlannerFamilyLoweringResult
    {
        return makeResult(PlannerAccessFamily::SEQ_SCAN,
                          AccessPathExactnessClass::EXACT_ROW,
                          AccessPathVisibilityEnforcement::INDEX_NATIVE,
                          false,
                          false,
                          true,
                          false);
    }

    auto lowerPlannerFamily(const PlannerFamilyLoweringRequest &request)
        -> PlannerFamilyLoweringResult
    {
        if (request.bitmap_combine)
        {
            return makeResult(PlannerAccessFamily::BITMAP_COMBINE_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false);
        }

        if (request.skip_scan && isBtreeLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::BTREE_SKIP_SCAN,
                              AccessPathExactnessClass::EXACT_KEY,
                              AccessPathVisibilityEnforcement::HYBRID,
                              false,
                              false,
                              true,
                              false);
        }

        if (isBtreeLike(request.index_type))
        {
            if (request.ordered_output)
            {
                return makeResult(PlannerAccessFamily::BTREE_ORDERED_SCAN,
                                  AccessPathExactnessClass::EXACT_KEY,
                                  AccessPathVisibilityEnforcement::HYBRID,
                                  false,
                                  true,
                                  true,
                                  true);
            }
            if (request.predicate_shape == PredicateMatchShape::RANGE ||
                request.predicate_shape == PredicateMatchShape::LIKE_PREFIX)
            {
                return makeResult(PlannerAccessFamily::BTREE_RANGE_SCAN,
                                  AccessPathExactnessClass::EXACT_KEY,
                                  AccessPathVisibilityEnforcement::HYBRID,
                                  false,
                                  false,
                                  true,
                                  true);
            }
            return makeResult(PlannerAccessFamily::BTREE_EQ_SCAN,
                              AccessPathExactnessClass::EXACT_KEY,
                              AccessPathVisibilityEnforcement::HYBRID,
                              false,
                              false,
                              true,
                              true);
        }

        if (isHashLike(request.index_type))
        {
            if (request.predicate_shape != PredicateMatchShape::EQUALITY)
            {
                return makeResult(PlannerAccessFamily::HASH_EQ_SCAN,
                                  AccessPathExactnessClass::UNKNOWN,
                                  AccessPathVisibilityEnforcement::UNKNOWN,
                                  true,
                                  false,
                                  false,
                                  false,
                                  AccessPathQueryabilityState::INVALID);
            }
            return makeResult(PlannerAccessFamily::HASH_EQ_SCAN,
                              AccessPathExactnessClass::EXACT_KEY,
                              AccessPathVisibilityEnforcement::HYBRID,
                              true,
                              false,
                              false,
                              false);
        }

        if (request.index_type == IndexType::LSM)
        {
            if (request.ordered_output &&
                (request.predicate_shape == PredicateMatchShape::RANGE ||
                 request.predicate_shape == PredicateMatchShape::LIKE_PREFIX))
            {
                return makeResult(PlannerAccessFamily::LSM_ORDERED_RANGE_SCAN,
                                  AccessPathExactnessClass::EXACT_KEY,
                                  AccessPathVisibilityEnforcement::HYBRID,
                                  false,
                                  true,
                                  false,
                                  true);
            }
            if (request.predicate_shape == PredicateMatchShape::RANGE ||
                request.predicate_shape == PredicateMatchShape::LIKE_PREFIX)
            {
                return makeResult(PlannerAccessFamily::LSM_RANGE_SCAN,
                                  AccessPathExactnessClass::EXACT_KEY,
                                  AccessPathVisibilityEnforcement::HYBRID,
                                  false,
                                  false,
                                  false,
                                  true);
            }
            return makeResult(PlannerAccessFamily::LSM_EQ_SCAN,
                              AccessPathExactnessClass::EXACT_KEY,
                              AccessPathVisibilityEnforcement::HYBRID,
                              false,
                              false,
                              false,
                              true);
        }

        if (isSummaryLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::BRIN_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false);
        }

        if (isSummaryFilterLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::SUMMARY_FILTER_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false);
        }

        if (request.index_type == IndexType::BITMAP ||
            request.index_type == IndexType::REDIS_BITMAP ||
            request.index_type == IndexType::NEO4J_LOOKUP)
        {
            return makeResult(PlannerAccessFamily::BITMAP_STORAGE_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (request.index_type == IndexType::COLUMNSTORE)
        {
            return makeResult(PlannerAccessFamily::COLUMNSTORE_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (request.index_type == IndexType::GIST)
        {
            return makeResult(request.nearest_order
                                  ? PlannerAccessFamily::GIST_NEAREST_SCAN
                                  : PlannerAccessFamily::GIST_SCAN,
                              request.nearest_order
                                  ? AccessPathExactnessClass::LOWER_BOUND_ORDERED
                                  : AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              request.nearest_order,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (request.index_type == IndexType::SPGIST)
        {
            return makeResult(request.nearest_order
                                  ? PlannerAccessFamily::SPGIST_NEAREST_SCAN
                                  : PlannerAccessFamily::SPGIST_SCAN,
                              request.nearest_order
                                  ? AccessPathExactnessClass::LOWER_BOUND_ORDERED
                                  : AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              request.nearest_order,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (request.index_type == IndexType::RTREE ||
            request.index_type == IndexType::MONGODB_2D ||
            request.index_type == IndexType::MONGODB_2DSPHERE ||
            request.index_type == IndexType::MONGODB_2DSPHERE_BUCKET ||
            request.index_type == IndexType::REDIS_GEO)
        {
            return makeResult(request.nearest_order
                                  ? PlannerAccessFamily::RTREE_NEAREST_SCAN
                                  : PlannerAccessFamily::RTREE_SCAN,
                              request.nearest_order
                                  ? AccessPathExactnessClass::LOWER_BOUND_ORDERED
                                  : AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              request.nearest_order,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (isTextLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::TEXT_RECHECK_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (request.index_type == IndexType::GIN)
        {
            return makeResult(PlannerAccessFamily::GIN_FILTER_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (isVectorFlatLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::VECTOR_FLAT_SCAN,
                              AccessPathExactnessClass::EXACT_KEY,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (isHnswLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::HNSW_SCAN,
                              AccessPathExactnessClass::APPROX_TOPK,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (isIvfLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::IVF_SCAN,
                              AccessPathExactnessClass::APPROX_TOPK,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        if (isGeneralizedLike(request.index_type))
        {
            return makeResult(PlannerAccessFamily::RTREE_SCAN,
                              AccessPathExactnessClass::CANDIDATE_REGION,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::LIMITED);
        }

        PlannerFamilyLoweringResult result;
        result.family = PlannerAccessFamily::UNKNOWN;
        result.family_name = "UNKNOWN";
        result.path_name = "UNKNOWN";
        result.exactness_class = AccessPathExactnessClass::UNKNOWN;
        result.visibility_enforcement = AccessPathVisibilityEnforcement::UNKNOWN;
        result.queryability_state = AccessPathQueryabilityState::INVALID;
        return result;
    }

} // namespace scratchbird::optimizer
