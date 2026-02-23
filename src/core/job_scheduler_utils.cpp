/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/job_scheduler_utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "scratchbird/core/timezone.h"

namespace scratchbird::core::detail {

namespace {

bool utcTimeFromEpoch(time_t value, std::tm& tm_out) {
#ifdef _WIN32
    return gmtime_s(&tm_out, &value) == 0;
#else
    return gmtime_r(&value, &tm_out) != nullptr;
#endif
}

std::vector<std::string> split(const std::string& input, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            out.push_back(item);
        }
    }
    return out;
}

bool parseCronNumber(const std::string& token, int& value_out) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    long val = std::strtol(token.c_str(), &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    value_out = static_cast<int>(val);
    return true;
}

bool parseCronField(const std::string& field, CronField& out) {
    out.allowed.assign(out.max_value - out.min_value + 1, false);
    out.any = false;

    if (field == "*") {
        out.any = true;
        std::fill(out.allowed.begin(), out.allowed.end(), true);
        return true;
    }

    auto parts = split(field, ',');
    if (parts.empty()) {
        return false;
    }

    for (const auto& part : parts) {
        std::string range_part = part;
        int step = 1;
        auto step_parts = split(part, '/');
        if (step_parts.size() == 2) {
            range_part = step_parts[0];
            if (!parseCronNumber(step_parts[1], step) || step <= 0) {
                return false;
            }
        } else if (step_parts.size() > 2) {
            return false;
        }

        int start = 0;
        int end = 0;
        if (range_part == "*") {
            start = out.min_value;
            end = out.max_value;
        } else {
            auto range_tokens = split(range_part, '-');
            if (range_tokens.size() == 1) {
                if (!parseCronNumber(range_tokens[0], start)) {
                    return false;
                }
                end = start;
            } else if (range_tokens.size() == 2) {
                if (!parseCronNumber(range_tokens[0], start) ||
                    !parseCronNumber(range_tokens[1], end)) {
                    return false;
                }
            } else {
                return false;
            }
        }

        if (start < out.min_value || end > out.max_value || start > end) {
            return false;
        }

        for (int value = start; value <= end; value += step) {
            out.allowed[value - out.min_value] = true;
        }
    }

    return true;
}

bool cronFieldMatches(const CronField& field, int value) {
    if (field.any) {
        return true;
    }
    if (value < field.min_value || value > field.max_value) {
        return false;
    }
    return field.allowed[value - field.min_value];
}

bool gmtimeUtc(time_t input, std::tm* out) {
#if defined(_WIN32)
    return out != nullptr && gmtime_s(out, &input) == 0;
#else
    return out != nullptr && gmtime_r(&input, out) != nullptr;
#endif
}

}  // namespace

bool parseCronExpression(const std::string& expr, CronExpression& out) {
    auto fields = split(expr, ' ');
    if (fields.size() != 5) {
        return false;
    }

    out.minute = {0, 59, true, {}};
    out.hour = {0, 23, true, {}};
    out.day_of_month = {1, 31, true, {}};
    out.month = {1, 12, true, {}};
    out.day_of_week = {0, 6, true, {}};

    if (!parseCronField(fields[0], out.minute)) return false;
    if (!parseCronField(fields[1], out.hour)) return false;
    if (!parseCronField(fields[2], out.day_of_month)) return false;
    if (!parseCronField(fields[3], out.month)) return false;
    if (!parseCronField(fields[4], out.day_of_week)) return false;

    return true;
}

bool cronMatches(const CronExpression& expr, const std::tm& tm) {
    int minute = tm.tm_min;
    int hour = tm.tm_hour;
    int dom = tm.tm_mday;
    int month = tm.tm_mon + 1;
    int dow = tm.tm_wday;

    if (!cronFieldMatches(expr.minute, minute)) return false;
    if (!cronFieldMatches(expr.hour, hour)) return false;
    if (!cronFieldMatches(expr.month, month)) return false;

    bool dom_any = expr.day_of_month.any;
    bool dow_any = expr.day_of_week.any;
    bool dom_match = cronFieldMatches(expr.day_of_month, dom);
    bool dow_match = cronFieldMatches(expr.day_of_week, dow);

    if (!dom_any && !dow_any) {
        return dom_match || dow_match;
    }
    if (!dom_any && dow_any) {
        return dom_match;
    }
    if (dom_any && !dow_any) {
        return dow_match;
    }
    return true;
}

uint64_t computeNextCronRunMs(const std::string& expr, uint64_t after_ms) {
    CronExpression parsed{};
    if (!parseCronExpression(expr, parsed)) {
        return 0;
    }

    constexpr int64_t kMaxMinutes = 60 * 24 * 366;
    int64_t after_seconds = static_cast<int64_t>(after_ms / 1000);
    int64_t candidate_seconds = after_seconds - (after_seconds % 60) + 60;

    for (int64_t minute = 0; minute < kMaxMinutes; ++minute) {
        time_t candidate = static_cast<time_t>(candidate_seconds + minute * 60);
        std::tm tm{};
        if (!gmtimeUtc(candidate, &tm)) {
            continue;
        }
        if (cronMatches(parsed, tm)) {
            return static_cast<uint64_t>(candidate) * 1000;
        }
    }

    return 0;
}

bool dependencySatisfied(const std::vector<CatalogManager::JobRunInfo>& runs) {
    if (runs.empty()) {
        return false;
    }
    const CatalogManager::JobRunInfo* latest = nullptr;
    for (const auto& run : runs) {
        if (!latest || run.completed_at > latest->completed_at) {
            latest = &run;
        }
    }
    return latest && latest->state == CatalogManager::JobRunState::COMPLETED;
}

uint64_t computeNextCronRunMsWithTimezone(const std::string& expr,
                                          uint64_t after_ms,
                                          const std::string& timezone_name) {
    CronExpression parsed{};
    if (!parseCronExpression(expr, parsed)) {
        return 0;
    }

    TimezoneManager& tz = getThreadLocalTimezoneManager();
    uint16_t tz_id = TimezoneManager::TZ_UTC;
    if (!timezone_name.empty()) {
        uint16_t by_name = tz.getTimezoneByName(timezone_name);
        uint16_t by_abbr = tz.getTimezoneByAbbreviation(timezone_name);
        if (by_name != 0) {
            tz_id = by_name;
        } else if (by_abbr != 0) {
            tz_id = by_abbr;
        }
    }

    constexpr int64_t kMaxMinutes = 60 * 24 * 366;
    int64_t after_seconds = static_cast<int64_t>(after_ms / 1000);
    int64_t candidate_seconds = after_seconds - (after_seconds % 60) + 60;

    for (int64_t minute = 0; minute < kMaxMinutes; ++minute) {
        time_t candidate = static_cast<time_t>(candidate_seconds + minute * 60);
        int64_t gmt_micro = static_cast<int64_t>(candidate) * 1000000;
        auto local_micro = tz.fromGMT(gmt_micro, tz_id, nullptr);
        if (!local_micro) {
            continue;
        }
        time_t local_seconds = static_cast<time_t>(*local_micro / 1000000);
        std::tm tm{};
        if (!gmtimeUtc(local_seconds, &tm)) {
            continue;
        }
        if (cronMatches(parsed, tm)) {
            return static_cast<uint64_t>(candidate) * 1000;
        }
    }

    return 0;
}

uint64_t computePreviousCronRunMsWithTimezone(const std::string& expr,
                                              uint64_t before_ms,
                                              const std::string& timezone_name) {
    CronExpression parsed{};
    if (!parseCronExpression(expr, parsed)) {
        return 0;
    }

    TimezoneManager& tz = getThreadLocalTimezoneManager();
    uint16_t tz_id = TimezoneManager::TZ_UTC;
    if (!timezone_name.empty()) {
        uint16_t by_name = tz.getTimezoneByName(timezone_name);
        uint16_t by_abbr = tz.getTimezoneByAbbreviation(timezone_name);
        if (by_name != 0) {
            tz_id = by_name;
        } else if (by_abbr != 0) {
            tz_id = by_abbr;
        }
    }

    constexpr int64_t kMaxMinutes = 60 * 24 * 366;
    int64_t before_seconds = static_cast<int64_t>(before_ms / 1000);
    int64_t candidate_seconds = before_seconds - (before_seconds % 60) - 60;

    for (int64_t minute = 0; minute < kMaxMinutes; ++minute) {
        time_t candidate = static_cast<time_t>(candidate_seconds - minute * 60);
        int64_t gmt_micro = static_cast<int64_t>(candidate) * 1000000;
        auto local_micro = tz.fromGMT(gmt_micro, tz_id, nullptr);
        if (!local_micro) {
            continue;
        }
        time_t local_seconds = static_cast<time_t>(*local_micro / 1000000);
        std::tm tm{};
        if (!gmtimeUtc(local_seconds, &tm)) {
            continue;
        }
        if (cronMatches(parsed, tm)) {
            return static_cast<uint64_t>(candidate) * 1000;
        }
    }

    return 0;
}

bool dependencySatisfiedForWindow(const std::vector<CatalogManager::JobRunInfo>& runs,
                                  uint64_t window_start_ms,
                                  uint64_t window_end_ms) {
    if (runs.empty()) {
        return false;
    }
    const CatalogManager::JobRunInfo* latest = nullptr;
    for (const auto& run : runs) {
        if (!latest || run.completed_at > latest->completed_at) {
            latest = &run;
        }
    }
    if (!latest) {
        return false;
    }
    if (latest->state != CatalogManager::JobRunState::COMPLETED) {
        return false;
    }
    if (latest->completed_at < window_start_ms || latest->completed_at > window_end_ms) {
        return false;
    }
    return true;
}

}  // namespace scratchbird::core::detail
