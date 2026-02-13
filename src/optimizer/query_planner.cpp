/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * QueryPlanner - Cost-based Query Planner Implementation
 *
 * V3 migration: resolved AST no longer exists. This planner is a placeholder
 * until a V3 resolver produces a fully-resolved query representation.
 */

#include "scratchbird/optimizer/query_planner.h"
#include "scratchbird/core/debug.h"

namespace scratchbird::optimizer
{

auto QueryPlanner::planQuery(const parser::v3::SelectStmt* select_stmt,
                             core::ErrorContext* ctx,
                             core::ConnectionContext* conn_ctx)
    -> std::shared_ptr<PlanNode>
{
    (void)select_stmt;
    (void)ctx;
    conn_ctx_ = conn_ctx;
    DEBUG_LOG_DB("QueryPlanner::planQuery - V3 resolver not available");
    return nullptr;
}

auto QueryPlanner::planAnalyze(const parser::v3::AnalyzeStmt* analyze_stmt,
                               core::ErrorContext* ctx)
    -> std::shared_ptr<PlanNode>
{
    (void)analyze_stmt;
    (void)ctx;
    DEBUG_LOG_DB("QueryPlanner::planAnalyze - V3 resolver not available");
    return nullptr;
}

} // namespace scratchbird::optimizer
