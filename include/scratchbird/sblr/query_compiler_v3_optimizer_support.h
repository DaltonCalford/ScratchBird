#pragma once

#include "scratchbird/core/database.h"
#include "scratchbird/optimizer/parameter_bindings.h"
#include "scratchbird/optimizer/vnext_plan_cache.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/sblr/v3_container.h"

#include <string>
#include <vector>

namespace scratchbird::sblr::detail
{
    enum class QueryCompilerV3PlanProfileMode : uint8_t
    {
        GENERIC = 0,
        CUSTOM = 1,
        AUTO = 2
    };

    struct QueryCompilerV3PlanProfile
    {
        QueryCompilerV3PlanProfileMode mode = QueryCompilerV3PlanProfileMode::GENERIC;
        bool parameter_sensitive = false;
        std::string signature = "GENERIC";
        std::string selectivity_bucket_signature;
        std::string runtime_plan_hash;
        std::string decision_source;
        std::string index_family_signature;
        std::string family_statistics_signature;
        std::string statistics_snapshot_signature;
        std::string cost_profile_id;
        std::string policy_snapshot_id;
    };

    struct QueryCompilerV3FinalizeResult
    {
        bool success = false;
        bool cache_hit = false;
        std::vector<uint8_t> bytecode;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        QueryCompilerV3PlanProfile plan_profile;
    };

    auto finalizeQueryCompilerV3Compilation(core::Database *db,
                                            const std::string &sql,
                                            const parser::v3::Statement *stmt,
                                            const parser::v3::StringPool &pool,
                                            const core::ID &current_schema_id,
                                            bool optimizations_enabled,
                                            sblr::v3::Container &container,
                                            const optimizer::ParameterBindings *parameter_bindings =
                                                nullptr,
                                            QueryCompilerV3PlanProfileMode
                                                plan_profile_mode =
                                                    QueryCompilerV3PlanProfileMode::GENERIC)
        -> QueryCompilerV3FinalizeResult;

    auto queryCompilerV3PlanCacheStats() -> optimizer::VNextPlanCacheStats;
    auto resetQueryCompilerV3PlanCacheStats() -> void;

} // namespace scratchbird::sblr::detail
