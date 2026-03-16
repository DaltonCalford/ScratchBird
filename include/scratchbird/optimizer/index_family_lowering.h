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
        PredicateMatchShape predicate_shape = PredicateMatchShape::NONE;
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

    auto lowerSequentialPlannerFamily() -> PlannerFamilyLoweringResult;

} // namespace scratchbird::optimizer
