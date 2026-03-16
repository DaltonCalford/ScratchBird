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

#include <algorithm>
#include <cctype>
#include <optional>

namespace scratchbird::optimizer
{
    namespace
    {
        using IndexType = core::CatalogManager::IndexType;
        using IndexOpclassFunctionKind =
            core::CatalogManager::IndexOpclassFunctionKind;

        auto toUpperAscii(std::string text) -> std::string
        {
            std::transform(text.begin(),
                           text.end(),
                           text.begin(),
                           [](unsigned char ch) {
                               return static_cast<char>(std::toupper(ch));
                           });
            return text;
        }

        auto isZeroId(const core::ID &id) -> bool
        {
            return std::all_of(id.bytes.begin(),
                               id.bytes.end(),
                               [](uint8_t byte) { return byte == 0; });
        }

        auto canonicalIndexTypeName(IndexType index_type) -> std::string
        {
            switch (index_type)
            {
                case IndexType::GIST: return "GIST";
                case IndexType::SPGIST: return "SPGIST";
                case IndexType::RTREE: return "RTREE";
                case IndexType::MONGODB_2D: return "MONGODB_2D";
                case IndexType::MONGODB_2DSPHERE: return "MONGODB_2DSPHERE";
                case IndexType::MONGODB_2DSPHERE_BUCKET:
                    return "MONGODB_2DSPHERE_BUCKET";
                case IndexType::REDIS_GEO: return "REDIS_GEO";
                default: return {};
            }
        }

        auto generalizedStrategyForOperator(const std::string &operator_name)
            -> std::optional<uint16_t>
        {
            const std::string normalized = toUpperAscii(operator_name);
            if (normalized == "=" || normalized == "EQUALS")
            {
                return 8;
            }
            if (normalized == "&&" || normalized == "OVERLAPS")
            {
                return 1;
            }
            if (normalized == "@>" || normalized == "CONTAINS")
            {
                return 2;
            }
            if (normalized == "<@" || normalized == "CONTAINED_BY")
            {
                return 3;
            }
            return std::nullopt;
        }

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

        auto invalidGeneralizedResult(PlannerAccessFamily family)
            -> PlannerFamilyLoweringResult
        {
            return makeResult(family,
                              AccessPathExactnessClass::UNKNOWN,
                              AccessPathVisibilityEnforcement::UNKNOWN,
                              true,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::INVALID);
        }

        auto invalidFamilyResult(PlannerAccessFamily family,
                                 bool requires_recheck = true)
            -> PlannerFamilyLoweringResult
        {
            return makeResult(family,
                              AccessPathExactnessClass::UNKNOWN,
                              AccessPathVisibilityEnforcement::UNKNOWN,
                              requires_recheck,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::INVALID);
        }

        auto primaryIndexOpclassId(core::CatalogManager *catalog,
                                   const core::CatalogManager::IndexInfo &index)
            -> core::ID
        {
            if (catalog == nullptr || isZeroId(index.index_id))
            {
                return {};
            }

            std::vector<core::CatalogManager::IndexColumnCatalogInfo> columns;
            if (catalog->listIndexColumnCatalogEntries(index.index_id, columns, nullptr) !=
                core::Status::OK)
            {
                return {};
            }

            for (const auto &column : columns)
            {
                if (!column.is_include)
                {
                    return column.opclass_id;
                }
            }

            return {};
        }

        auto hasOpclassFunction(core::CatalogManager *catalog,
                                const core::ID &opclass_id,
                                IndexOpclassFunctionKind fn_kind,
                                std::optional<uint16_t> support_number =
                                    std::nullopt)
            -> bool
        {
            if (catalog == nullptr || isZeroId(opclass_id))
            {
                return false;
            }

            std::vector<core::CatalogManager::IndexOpclassFunctionCatalogInfo> rows;
            if (catalog->listIndexOpclassFunctionCatalogEntries(opclass_id, rows, nullptr) !=
                core::Status::OK)
            {
                return false;
            }

            return std::any_of(
                rows.begin(),
                rows.end(),
                [&](const core::CatalogManager::IndexOpclassFunctionCatalogInfo &row) {
                    return row.is_valid && row.fn_kind == fn_kind &&
                           (!support_number.has_value() ||
                            row.support_number == *support_number);
                });
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
            if (!request.strategy_bound || !request.support_consistent)
            {
                return invalidGeneralizedResult(request.nearest_order
                                                    ? PlannerAccessFamily::GIST_NEAREST_SCAN
                                                    : PlannerAccessFamily::GIST_SCAN);
            }
            if (request.nearest_order &&
                (!request.support_distance ||
                 !request.nearest_lower_bound_validated))
            {
                return invalidGeneralizedResult(
                    PlannerAccessFamily::GIST_NEAREST_SCAN);
            }
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
            if (!request.strategy_bound || !request.support_consistent)
            {
                return invalidGeneralizedResult(request.nearest_order
                                                    ? PlannerAccessFamily::SPGIST_NEAREST_SCAN
                                                    : PlannerAccessFamily::SPGIST_SCAN);
            }
            if (request.nearest_order &&
                (!request.support_distance ||
                 !request.nearest_lower_bound_validated))
            {
                return invalidGeneralizedResult(
                    PlannerAccessFamily::SPGIST_NEAREST_SCAN);
            }
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
            if (!request.strategy_bound || !request.support_consistent)
            {
                return invalidGeneralizedResult(request.nearest_order
                                                    ? PlannerAccessFamily::RTREE_NEAREST_SCAN
                                                    : PlannerAccessFamily::RTREE_SCAN);
            }
            if (request.nearest_order &&
                (!request.support_distance ||
                 !request.nearest_lower_bound_validated))
            {
                return invalidGeneralizedResult(
                    PlannerAccessFamily::RTREE_NEAREST_SCAN);
            }
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
            if (request.ranking_requested)
            {
                if (!request.corpus_stats_available ||
                    request.candidate_budget == 0)
                {
                    return invalidFamilyResult(
                        PlannerAccessFamily::TEXT_SCORE_SCAN);
                }

                return makeResult(PlannerAccessFamily::TEXT_SCORE_SCAN,
                                  AccessPathExactnessClass::CANDIDATE_REGION,
                                  AccessPathVisibilityEnforcement::POST_FILTER,
                                  true,
                                  false,
                                  false,
                                  false,
                                  AccessPathQueryabilityState::LIMITED);
            }

            if (request.predicate_shape == PredicateMatchShape::LIKE_PREFIX)
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

            if (request.predicate_shape == PredicateMatchShape::EQUALITY ||
                request.candidate_bitmap_available)
            {
                return makeResult(PlannerAccessFamily::TEXT_BITMAP_SCAN,
                                  AccessPathExactnessClass::CANDIDATE_REGION,
                                  AccessPathVisibilityEnforcement::POST_FILTER,
                                  true,
                                  false,
                                  false,
                                  false,
                                  AccessPathQueryabilityState::LIMITED);
            }

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
            if (!request.nearest_order ||
                !request.ann_metric_compatible ||
                request.candidate_budget == 0)
            {
                return invalidFamilyResult(
                    PlannerAccessFamily::VECTOR_FLAT_SCAN,
                    false);
            }

            return makeResult(PlannerAccessFamily::VECTOR_FLAT_SCAN,
                              AccessPathExactnessClass::EXACT_ROW,
                              AccessPathVisibilityEnforcement::POST_FILTER,
                              false,
                              false,
                              false,
                              false,
                              AccessPathQueryabilityState::QUERYABLE);
        }

        if (isHnswLike(request.index_type))
        {
            if (!request.nearest_order ||
                !request.ann_metric_compatible ||
                request.candidate_budget == 0)
            {
                return invalidFamilyResult(PlannerAccessFamily::HNSW_SCAN);
            }

            if (request.ann_exact_fallback)
            {
                return makeResult(
                    PlannerAccessFamily::ANN_HYBRID_FALLBACK_SCAN,
                    AccessPathExactnessClass::EXACT_ROW,
                    AccessPathVisibilityEnforcement::POST_FILTER,
                    false,
                    false,
                    false,
                    false,
                    AccessPathQueryabilityState::QUERYABLE);
            }

            if (request.ann_rerank_enabled)
            {
                return makeResult(PlannerAccessFamily::ANN_RERANK_SCAN,
                                  AccessPathExactnessClass::APPROX_TOPK,
                                  AccessPathVisibilityEnforcement::POST_FILTER,
                                  true,
                                  false,
                                  false,
                                  false,
                                  AccessPathQueryabilityState::LIMITED);
            }

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
            if (!request.nearest_order ||
                !request.ann_metric_compatible ||
                request.candidate_budget == 0)
            {
                return invalidFamilyResult(PlannerAccessFamily::IVF_SCAN);
            }

            if (request.ann_exact_fallback)
            {
                return makeResult(
                    PlannerAccessFamily::ANN_HYBRID_FALLBACK_SCAN,
                    AccessPathExactnessClass::EXACT_ROW,
                    AccessPathVisibilityEnforcement::POST_FILTER,
                    false,
                    false,
                    false,
                    false,
                    AccessPathQueryabilityState::QUERYABLE);
            }

            if (request.ann_rerank_enabled)
            {
                return makeResult(PlannerAccessFamily::ANN_RERANK_SCAN,
                                  AccessPathExactnessClass::APPROX_TOPK,
                                  AccessPathVisibilityEnforcement::POST_FILTER,
                                  true,
                                  false,
                                  false,
                                  false,
                                  AccessPathQueryabilityState::LIMITED);
            }

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

    auto buildPlannerFamilyLoweringRequest(core::CatalogManager *catalog,
                                           const core::CatalogManager::IndexInfo &index,
                                           PredicateMatchShape predicate_shape,
                                           const std::string &operator_name,
                                           bool ordered_output,
                                           bool skip_scan,
                                           bool bitmap_combine,
                                           bool nearest_order)
        -> PlannerFamilyLoweringRequest
    {
        PlannerFamilyLoweringRequest request;
        request.index_type = index.index_type;
        request.ordered_output = ordered_output;
        request.skip_scan = skip_scan;
        request.bitmap_combine = bitmap_combine;
        request.nearest_order = nearest_order;
        request.predicate_shape = predicate_shape;

        if (index.index_type == IndexType::GIST ||
            index.index_type == IndexType::SPGIST)
        {
            const core::ID opclass_id = primaryIndexOpclassId(catalog, index);
            if (isZeroId(opclass_id))
            {
                return request;
            }

            core::CatalogManager::IndexOpclassCatalogInfo opclass_info;
            if (catalog == nullptr ||
                catalog->getIndexOpclassCatalogEntry(opclass_id, opclass_info, nullptr) !=
                    core::Status::OK)
            {
                return request;
            }

            if (toUpperAscii(opclass_info.index_type_name) !=
                canonicalIndexTypeName(index.index_type))
            {
                return request;
            }

            const auto strategy = generalizedStrategyForOperator(operator_name);
            if (!strategy.has_value())
            {
                return request;
            }

            request.strategy_number = *strategy;
            request.strategy_bound =
                hasOpclassFunction(catalog,
                                   opclass_id,
                                   IndexOpclassFunctionKind::CONSISTENT,
                                   request.strategy_number);
            request.support_consistent = request.strategy_bound;
            request.support_distance =
                hasOpclassFunction(catalog,
                                   opclass_id,
                                   IndexOpclassFunctionKind::DISTANCE);
            request.nearest_lower_bound_validated = request.support_distance;
            return request;
        }

        if (index.index_type == IndexType::RTREE ||
            index.index_type == IndexType::MONGODB_2D ||
            index.index_type == IndexType::MONGODB_2DSPHERE ||
            index.index_type == IndexType::MONGODB_2DSPHERE_BUCKET ||
            index.index_type == IndexType::REDIS_GEO)
        {
            const auto strategy = generalizedStrategyForOperator(operator_name);
            if (!strategy.has_value())
            {
                return request;
            }

            request.strategy_number = *strategy;
            request.strategy_bound = true;
            request.support_consistent = true;
            request.support_distance = false;
            request.nearest_lower_bound_validated = false;
        }

        return request;
    }

} // namespace scratchbird::optimizer
