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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/optimizer/path.h"

#include <string>

namespace scratchbird::optimizer
{

    inline constexpr const char *kBaseRelationCandidateBundleContractId =
        "sb_base_relation_candidate_bundle/v1";
    inline constexpr const char *kBaseRelationCandidateBundleOwnerPassId =
        "P08_ACCESS_PATH_ANNOTATE";
    inline constexpr const char *kBaseRelationCandidateBundleConsumerPassId =
        "P09_JOIN_ORDER_PLAN";
    inline constexpr const char *kMgaRecheckContractId =
        "sb_mga_recheck/v1";

    enum class PredicateMatchShape : uint8_t
    {
        NONE = 0,
        EQUALITY = 1,
        RANGE = 2,
        LIKE_PREFIX = 3,
    };

    struct PlannerFamilyLoweringRequest
    {
        core::CatalogManager::IndexType index_type =
            core::CatalogManager::IndexType::BTREE;
        bool ordered_output = false;
        bool skip_scan = false;
        bool bitmap_combine = false;
        bool nearest_order = false;
        bool ranking_requested = false;
        bool corpus_stats_available = false;
        bool candidate_bitmap_available = false;
        PredicateMatchShape predicate_shape = PredicateMatchShape::NONE;
        bool strategy_bound = false;
        uint16_t strategy_number = 0;
        bool support_consistent = false;
        bool support_distance = false;
        bool nearest_lower_bound_validated = false;
        uint64_t candidate_budget = 0;
        bool ann_metric_compatible = false;
        bool ann_rerank_enabled = false;
        bool ann_exact_fallback = false;
    };

    struct PlannerFamilyLoweringResult
    {
        PlannerAccessFamily family = PlannerAccessFamily::UNKNOWN;
        std::string family_name;
        std::string path_name;
        AccessPathExactnessClass exactness_class =
            AccessPathExactnessClass::UNKNOWN;
        AccessPathVisibilityEnforcement visibility_enforcement =
            AccessPathVisibilityEnforcement::UNKNOWN;
        AccessPathQueryabilityState queryability_state =
            AccessPathQueryabilityState::UNKNOWN;
        bool requires_recheck = false;
        bool supports_ordering = false;
        bool supports_covering = false;
        bool supports_parameterization = false;
    };

    auto lowerPlannerFamily(const PlannerFamilyLoweringRequest &request)
        -> PlannerFamilyLoweringResult;

    auto buildPlannerFamilyLoweringRequest(core::CatalogManager *catalog,
                                           const core::CatalogManager::IndexInfo &index,
                                           PredicateMatchShape predicate_shape,
                                           const std::string &operator_name,
                                           bool ordered_output = false,
                                           bool skip_scan = false,
                                           bool bitmap_combine = false,
                                           bool nearest_order = false)
        -> PlannerFamilyLoweringRequest;

    auto lowerSequentialPlannerFamily() -> PlannerFamilyLoweringResult;

    inline auto plannerMgaVisibilityStateName(
        AccessPathVisibilityEnforcement enforcement) -> const char *
    {
        switch (enforcement)
        {
            case AccessPathVisibilityEnforcement::INDEX_NATIVE:
                return "INDEX_NATIVE";
            case AccessPathVisibilityEnforcement::POST_FILTER:
                return "HEAP_POST_FILTER";
            case AccessPathVisibilityEnforcement::HYBRID:
                return "HYBRID_INDEX_HEAP";
            case AccessPathVisibilityEnforcement::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }

    inline auto plannerFamilyLoweringRejectionCode(
        const PlannerFamilyLoweringRequest &request,
        const PlannerFamilyLoweringResult &result) -> const char *
    {
        if (result.queryability_state != AccessPathQueryabilityState::INVALID)
        {
            return "";
        }
        if (request.bitmap_combine)
        {
            return "P08_BITMAP_COMPOSE_UNAVAILABLE";
        }
        if (request.skip_scan)
        {
            return "P08_SKIP_SCAN_UNAVAILABLE";
        }
        if (request.nearest_order && !request.ann_metric_compatible)
        {
            return "P08_ANN_METRIC_INCOMPATIBLE";
        }
        if (request.nearest_order && !request.nearest_lower_bound_validated)
        {
            return "P08_NEAREST_ORDER_UNVALIDATED";
        }
        if (request.strategy_bound == false &&
            (request.support_consistent || request.support_distance ||
             request.nearest_order))
        {
            return "P08_OPERATOR_STRATEGY_UNBOUND";
        }
        return "P08_FAMILY_NOT_QUERYABLE";
    }

    inline auto plannerFamilyLoweringRejectionDetail(
        const PlannerFamilyLoweringRequest &request,
        const PlannerFamilyLoweringResult &result) -> std::string
    {
        (void)result;
        if (request.bitmap_combine)
        {
            return "bitmap composition could not be lowered to a queryable planner family";
        }
        if (request.skip_scan)
        {
            return "skip-scan candidate did not lower to a queryable skip family";
        }
        if (request.nearest_order && !request.ann_metric_compatible)
        {
            return "nearest-order candidate was rejected because ANN metric compatibility was not proven";
        }
        if (request.nearest_order && !request.nearest_lower_bound_validated)
        {
            return "nearest-order candidate was rejected because lower-bound ordering was not validated";
        }
        if (request.strategy_bound == false &&
            (request.support_consistent || request.support_distance ||
             request.nearest_order))
        {
            return "generalized or nearest strategy binding was not available for the requested operator";
        }
        return "planner family lowering marked the candidate as invalid for query execution";
    }

} // namespace scratchbird::optimizer
