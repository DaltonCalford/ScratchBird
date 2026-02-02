/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/timezone_loader.h"
#include <ctime>
#include <cstring>
#include <algorithm>
#include <map>

namespace scratchbird::core
{

    TimezoneLoader::TimezoneLoader(CatalogManager *catalog)
        : catalog_(catalog), next_timezone_id_(100)
    {
    }

    auto TimezoneLoader::getCurrentTimestamp() -> int64_t
    {
        return static_cast<int64_t>(std::time(nullptr));
    }

    auto TimezoneLoader::calculateWeekOfMonth(int day) -> uint8_t
    {
        if (day >= 1 && day <= 7)
            return 1; // First week
        if (day >= 8 && day <= 14)
            return 2; // Second week
        if (day >= 15 && day <= 21)
            return 3; // Third week
        if (day >= 22 && day <= 28)
            return 4; // Fourth week
        return 0; // Last week (5th week or end of month)
    }

    void TimezoneLoader::extractDSTRules(const std::vector<TimezoneTransition> &transitions,
                                         CatalogManager::TimezoneInfo &tz_info)
    {
        if (transitions.empty())
        {
            // No transitions - no DST
            tz_info.observes_dst = false;
            return;
        }

        // Look at recent transitions (last 5 years)
        int64_t five_years_ago = getCurrentTimestamp() - (5 * 365 * 24 * 3600);

        // Find DST start and end transitions
        struct DSTTransition
        {
            int month = 0;
            int day = 0;
            int hour = 0;
            bool is_dst_start = false;
        };

        std::vector<DSTTransition> dst_starts;
        std::vector<DSTTransition> dst_ends;

        for (size_t i = 0; i < transitions.size(); i++)
        {
            const auto &trans = transitions[i];

            // Only look at recent transitions
            if (trans.timestamp < five_years_ago)
            {
                continue;
            }

            // Convert timestamp to date components
            time_t tt = static_cast<time_t>(trans.timestamp);
            struct tm *timeinfo = gmtime(&tt);
            if (!timeinfo)
            {
                continue;
            }

            DSTTransition dst_trans;
            dst_trans.month = timeinfo->tm_mon + 1; // 0-11 to 1-12
            dst_trans.day = timeinfo->tm_mday;
            dst_trans.hour = timeinfo->tm_hour;

            if (trans.is_dst)
            {
                // Transition TO DST (spring forward)
                dst_trans.is_dst_start = true;
                dst_starts.push_back(dst_trans);
            }
            else if (i > 0 && transitions[i - 1].is_dst)
            {
                // Transition FROM DST (fall back)
                dst_trans.is_dst_start = false;
                dst_ends.push_back(dst_trans);
            }
        }

        if (!dst_starts.empty() && !dst_ends.empty())
        {
            // We have DST transitions
            tz_info.observes_dst = true;

            // Use most recent transition as the pattern
            const auto &start = dst_starts.back();
            const auto &end = dst_ends.back();

            tz_info.dst_start_month = static_cast<uint8_t>(start.month);
            tz_info.dst_start_week = calculateWeekOfMonth(start.day);
            tz_info.dst_start_day = 0; // Phase 4 Enhancement: Calculate day of week from date
            tz_info.dst_start_hour = static_cast<uint8_t>(start.hour);

            tz_info.dst_end_month = static_cast<uint8_t>(end.month);
            tz_info.dst_end_week = calculateWeekOfMonth(end.day);
            tz_info.dst_end_day = 0; // Phase 4 Enhancement: Calculate day of week from date
            tz_info.dst_end_hour = static_cast<uint8_t>(end.hour);

            // Calculate DST offset (typically +60 minutes)
            // Find a DST transition to get the offset
            for (const auto &trans : transitions)
            {
                if (trans.is_dst)
                {
                    int32_t dst_offset_seconds = trans.utc_offset;
                    tz_info.dst_offset_minutes = dst_offset_seconds / 60;
                    break;
                }
            }
        }
        else
        {
            tz_info.observes_dst = false;
        }
    }

    void TimezoneLoader::convertToSimplifiedDST(const TimezoneData &tz_data,
                                                CatalogManager::TimezoneInfo &tz_info)
    {
        // Initialize with defaults
        tz_info.timezone_id = next_timezone_id_++;
        tz_info.name = tz_data.name;
        tz_info.abbreviation = "";
        tz_info.std_offset_minutes = 0;
        tz_info.observes_dst = false;
        tz_info.dst_start_month = 0;
        tz_info.dst_start_week = 0;
        tz_info.dst_start_day = 0;
        tz_info.dst_start_hour = 0;
        tz_info.dst_end_month = 0;
        tz_info.dst_end_week = 0;
        tz_info.dst_end_day = 0;
        tz_info.dst_end_hour = 0;
        tz_info.dst_offset_minutes = 0;
        tz_info.created_time = static_cast<uint32_t>(getCurrentTimestamp());
        tz_info.last_modified_time = tz_info.created_time;

        if (tz_data.transitions.empty())
        {
            // No transitions - likely a fixed-offset timezone like UTC
            tz_info.std_offset_minutes = 0;
            tz_info.abbreviation = tz_data.name;
            return;
        }

        // Find the most common non-DST offset (standard time)
        std::map<int32_t, int> offset_counts;
        for (const auto &trans : tz_data.transitions)
        {
            if (!trans.is_dst)
            {
                int32_t offset_minutes = trans.utc_offset / 60;
                offset_counts[offset_minutes]++;
            }
        }

        if (!offset_counts.empty())
        {
            // Use most common non-DST offset
            auto max_it = std::max_element(
                offset_counts.begin(), offset_counts.end(),
                [](const auto &a, const auto &b)
                { return a.second < b.second; });
            tz_info.std_offset_minutes = max_it->first;
        }
        else
        {
            // Fall back to first transition's offset
            tz_info.std_offset_minutes = tz_data.transitions[0].utc_offset / 60;
        }

        // Get abbreviation from most recent transition
        if (!tz_data.transitions.empty())
        {
            const auto &last_trans = tz_data.transitions.back();
            if (!last_trans.abbreviation.empty())
            {
                tz_info.abbreviation = last_trans.abbreviation;
            }
            else
            {
                // Use timezone name if no abbreviation
                tz_info.abbreviation = tz_data.name;
            }
        }

        // Extract DST rules from transitions
        extractDSTRules(tz_data.transitions, tz_info);
    }

    auto TimezoneLoader::loadTimezone(const TimezoneData &tz_data, ErrorContext *ctx) -> Status
    {
        if (tz_data.name.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Timezone name cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        // Convert full timezone data to simplified DST model
        CatalogManager::TimezoneInfo tz_info;
        convertToSimplifiedDST(tz_data, tz_info);

        // Create timezone in catalog
        Status status = catalog_->createTimezone(tz_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        return Status::OK;
    }

    auto TimezoneLoader::loadFromFile(const std::string &filepath, ErrorContext *ctx) -> Status
    {
        // Parse TZif file
        TimezoneData tz_data;
        Status status = parser_.parseFile(filepath, tz_data, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Load into catalog
        return loadTimezone(tz_data, ctx);
    }

    auto TimezoneLoader::loadFromDirectory(const std::string &zoneinfo_dir,
                                           ErrorContext *ctx) -> Status
    {
        // Parse all timezone files in directory
        std::vector<TimezoneData> timezones;
        Status status = parser_.parseDirectory(zoneinfo_dir, timezones, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (timezones.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No timezone files found in directory");
            return Status::NOT_FOUND;
        }

        // Load each timezone into catalog
        size_t loaded_count = 0;
        size_t failed_count = 0;

        for (const auto &tz_data : timezones)
        {
            ErrorContext local_ctx;
            status = loadTimezone(tz_data, &local_ctx);
            if (status == Status::OK)
            {
                loaded_count++;
            }
            else
            {
                failed_count++;
                // Log warning but continue (don't fail entire operation)
                // In production, this would use proper logging
            }
        }

        if (loaded_count == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                              "Failed to load any timezones from directory");
            return Status::INTERNAL_ERROR;
        }

        // Return OK if at least some timezones were loaded
        return Status::OK;
    }

    auto TimezoneLoader::clearAllTimezones(ErrorContext *ctx) -> Status
    {
        if (!catalog_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::TimezoneInfo> timezones;
        Status status = catalog_->listTimezones(timezones, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        for (const auto& tz : timezones)
        {
            ErrorContext local_ctx;
            status = catalog_->deleteTimezone(tz.timezone_id, &local_ctx);
            if (status != Status::OK)
            {
                if (ctx && ctx->message.empty())
                {
                    ctx->code = local_ctx.code;
                    ctx->sqlstate = local_ctx.sqlstate;
                    ctx->message = local_ctx.message;
                    ctx->error_message = ctx->message;
                    ctx->file = local_ctx.file;
                    ctx->line = local_ctx.line;
                    ctx->function = local_ctx.function;
                    ctx->constraint_name = local_ctx.constraint_name;
                    ctx->table_name = local_ctx.table_name;
                    ctx->column_name = local_ctx.column_name;
                    ctx->violating_value = local_ctx.violating_value;
                    ctx->referenced_table = local_ctx.referenced_table;
                    ctx->referenced_column = local_ctx.referenced_column;
                    ctx->check_expression = local_ctx.check_expression;
                    ctx->hint = local_ctx.hint;
                    ctx->cause = nullptr;
                }
                return status;
            }
        }

        return Status::OK;
    }

    auto TimezoneLoader::getLoadedTimezoneStats(size_t &total_count,
                                                size_t &with_dst_count,
                                                ErrorContext *ctx) -> Status
    {
        total_count = 0;
        with_dst_count = 0;

        if (!catalog_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::TimezoneInfo> timezones;
        Status status = catalog_->listTimezones(timezones, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        total_count = timezones.size();
        for (const auto& tz : timezones)
        {
            if (tz.observes_dst)
            {
                ++with_dst_count;
            }
        }

        return Status::OK;
    }

} // namespace scratchbird::core
