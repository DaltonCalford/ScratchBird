#pragma once

#include "scratchbird/core/database.h"
#include "scratchbird/optimizer/vnext_plan_cache.h"
#include "scratchbird/parser/ast_v3.h"
#include "scratchbird/sblr/v3_container.h"

#include <string>
#include <vector>

namespace scratchbird::sblr::detail
{
    struct QueryCompilerV3FinalizeResult
    {
        bool success = false;
        bool cache_hit = false;
        std::vector<uint8_t> bytecode;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    auto finalizeQueryCompilerV3Compilation(core::Database *db,
                                            const std::string &sql,
                                            const parser::v3::Statement *stmt,
                                            const parser::v3::StringPool &pool,
                                            const core::ID &current_schema_id,
                                            bool optimizations_enabled,
                                            sblr::v3::Container &container)
        -> QueryCompilerV3FinalizeResult;

    auto queryCompilerV3PlanCacheStats() -> optimizer::VNextPlanCacheStats;
    auto resetQueryCompilerV3PlanCacheStats() -> void;

} // namespace scratchbird::sblr::detail
