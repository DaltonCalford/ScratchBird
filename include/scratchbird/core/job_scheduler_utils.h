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

bool dependencySatisfied(const std::vector<CatalogManager::JobRunInfo>& runs);

}  // namespace scratchbird::core::detail
