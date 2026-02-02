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

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"

namespace scratchbird::core::detail {

struct CronField {
    int min_value = 0;
    int max_value = 0;
    bool any = true;
    std::vector<bool> allowed;
};

struct CronExpression {
    CronField minute;
    CronField hour;
    CronField day_of_month;
    CronField month;
    CronField day_of_week;
};

bool parseCronExpression(const std::string& expr, CronExpression& out);
bool cronMatches(const CronExpression& expr, const std::tm& tm);
uint64_t computeNextCronRunMs(const std::string& expr, uint64_t after_ms);
uint64_t computeNextCronRunMsWithTimezone(const std::string& expr,
                                          uint64_t after_ms,
                                          const std::string& timezone_name);
uint64_t computePreviousCronRunMsWithTimezone(const std::string& expr,
                                              uint64_t before_ms,
                                              const std::string& timezone_name);

bool dependencySatisfied(const std::vector<CatalogManager::JobRunInfo>& runs);
bool dependencySatisfiedForWindow(const std::vector<CatalogManager::JobRunInfo>& runs,
                                  uint64_t window_start_ms,
                                  uint64_t window_end_ms);

}  // namespace scratchbird::core::detail
