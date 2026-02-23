/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/catalog_manager.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>

namespace scratchbird::core
{
    namespace
    {
        bool gmtimeUtc(time_t input, struct tm* out)
        {
#if defined(_WIN32)
            return out != nullptr && gmtime_s(out, &input) == 0;
#else
            return out != nullptr && gmtime_r(&input, out) != nullptr;
#endif
        }

        time_t timegmPortable(struct tm* input)
        {
#if defined(_WIN32)
            return _mkgmtime(input);
#else
            return timegm(input);
#endif
        }
    } // namespace

    // ===== TimezoneOffset Implementation =====

    std::string TimezoneOffset::toString() const
    {
        int hours = offset_minutes / 60;
        int minutes = std::abs(offset_minutes % 60);

        std::ostringstream oss;
        oss << (offset_minutes >= 0 ? '+' : '-') << std::setfill('0') << std::setw(2)
            << std::abs(hours) << ':' << std::setfill('0') << std::setw(2) << minutes;
        return oss.str();
    }

    auto TimezoneOffset::fromString(const std::string &str, ErrorContext *ctx)
        -> std::optional<TimezoneOffset>
    {
        if (str.empty())
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty timezone offset string");
            }
            return std::nullopt;
        }

        // Parse format: +/-HH:MM or +/-HHMM
        int sign = 1;
        size_t pos = 0;

        if (str[0] == '+')
        {
            sign = 1;
            pos = 1;
        }
        else if (str[0] == '-')
        {
            sign = -1;
            pos = 1;
        }

        int hours = 0;
        int minutes = 0;

        if (str.find(':') != std::string::npos)
        {
            // Format: +/-HH:MM
            if (sscanf(str.c_str() + pos, "%d:%d", &hours, &minutes) != 2)
            {
                if (ctx)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid timezone offset format");
                }
                return std::nullopt;
            }
        }
        else
        {
            // Format: +/-HHMM
            if (str.length() - pos != 4)
            {
                if (ctx)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid timezone offset format");
                }
                return std::nullopt;
            }
            hours = std::stoi(str.substr(pos, 2));
            minutes = std::stoi(str.substr(pos + 2, 2));
        }

        // Validate individual components
        if (hours < 0 || minutes < 0 || minutes > 59)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Timezone offset component out of range");
            }
            return std::nullopt;
        }

        // Validate total offset: -12:00 to +14:00 (-720 to +840 minutes)
        int total_minutes = sign * (hours * 60 + minutes);
        if (total_minutes < -720 || total_minutes > 840)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(
                    ctx, Status::INVALID_ARGUMENT,
                    "Timezone offset out of range (must be between -12:00 and +14:00)");
            }
            return std::nullopt;
        }

        return TimezoneOffset(total_minutes, false);
    }

    // ===== TimezoneManager Implementation =====

    TimezoneManager::TimezoneManager()
    {
        initializeTimezones();
    }

    TimezoneManager::~TimezoneManager() = default;

    void TimezoneManager::initializeTimezones()
    {
        timezones_.clear();
        name_to_id_.clear();
        abbr_to_id_.clear();

        // UTC/GMT (ID 1)
        TimezoneInfo utc;
        utc.timezone_id = 1;
        utc.name = "UTC";
        utc.abbreviation = "UTC";
        utc.offset = TimezoneOffset(0, false);
        utc.observes_dst = false;
        utc.dst_offset_minutes = 0;
        timezones_[1] = utc;
        name_to_id_["UTC"] = 1;
        name_to_id_["GMT"] = 1;
        abbr_to_id_["UTC"] = 1;
        abbr_to_id_["GMT"] = 1;

        // EST - Eastern Standard Time (ID 2)
        TimezoneInfo est;
        est.timezone_id = 2;
        est.name = "America/New_York";
        est.abbreviation = "EST";
        est.offset = TimezoneOffset(-5 * 60, false); // -05:00
        est.observes_dst = true;
        est.dst_start_month = 3;
        est.dst_start_week = 2;
        est.dst_start_day = 0;
        est.dst_start_hour = 2;
        est.dst_end_month = 11;
        est.dst_end_week = 1;
        est.dst_end_day = 0;
        est.dst_end_hour = 2;
        est.dst_offset_minutes = 60;
        timezones_[2] = est;
        name_to_id_["America/New_York"] = 2;
        name_to_id_["EST"] = 2;
        name_to_id_["EDT"] = 2;
        abbr_to_id_["EST"] = 2;
        abbr_to_id_["EDT"] = 2;

        // PST - Pacific Standard Time (ID 3)
        TimezoneInfo pst;
        pst.timezone_id = 3;
        pst.name = "America/Los_Angeles";
        pst.abbreviation = "PST";
        pst.offset = TimezoneOffset(-8 * 60, false); // -08:00
        pst.observes_dst = true;
        pst.dst_start_month = 3;
        pst.dst_start_week = 2;
        pst.dst_start_day = 0;
        pst.dst_start_hour = 2;
        pst.dst_end_month = 11;
        pst.dst_end_week = 1;
        pst.dst_end_day = 0;
        pst.dst_end_hour = 2;
        pst.dst_offset_minutes = 60;
        timezones_[3] = pst;
        name_to_id_["America/Los_Angeles"] = 3;
        name_to_id_["PST"] = 3;
        name_to_id_["PDT"] = 3;
        abbr_to_id_["PST"] = 3;
        abbr_to_id_["PDT"] = 3;

        // CST - Central Standard Time (ID 4)
        TimezoneInfo cst;
        cst.timezone_id = 4;
        cst.name = "America/Chicago";
        cst.abbreviation = "CST";
        cst.offset = TimezoneOffset(-6 * 60, false); // -06:00
        cst.observes_dst = true;
        cst.dst_start_month = 3;
        cst.dst_start_week = 2;
        cst.dst_start_day = 0;
        cst.dst_start_hour = 2;
        cst.dst_end_month = 11;
        cst.dst_end_week = 1;
        cst.dst_end_day = 0;
        cst.dst_end_hour = 2;
        cst.dst_offset_minutes = 60;
        timezones_[4] = cst;
        name_to_id_["America/Chicago"] = 4;
        name_to_id_["CST"] = 4;
        name_to_id_["CDT"] = 4;
        abbr_to_id_["CST"] = 4;
        abbr_to_id_["CDT"] = 4;

        // MST - Mountain Standard Time (ID 5)
        TimezoneInfo mst;
        mst.timezone_id = 5;
        mst.name = "America/Denver";
        mst.abbreviation = "MST";
        mst.offset = TimezoneOffset(-7 * 60, false); // -07:00
        mst.observes_dst = true;
        mst.dst_start_month = 3;
        mst.dst_start_week = 2;
        mst.dst_start_day = 0;
        mst.dst_start_hour = 2;
        mst.dst_end_month = 11;
        mst.dst_end_week = 1;
        mst.dst_end_day = 0;
        mst.dst_end_hour = 2;
        mst.dst_offset_minutes = 60;
        timezones_[5] = mst;
        name_to_id_["America/Denver"] = 5;
        name_to_id_["MST"] = 5;
        name_to_id_["MDT"] = 5;
        abbr_to_id_["MST"] = 5;
        abbr_to_id_["MDT"] = 5;
    }

    auto TimezoneManager::loadFromCatalog(CatalogManager* catalog, ErrorContext* ctx) -> Status
    {
        if (catalog == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Catalog manager not available for timezone load");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::TimezoneInfo> tz_list;
        Status status = catalog->listTimezones(tz_list, ctx);

        if (status == Status::OK && !tz_list.empty())
        {
            timezones_.clear();
            name_to_id_.clear();
            abbr_to_id_.clear();

            for (const auto& tz : tz_list)
            {
                TimezoneInfo info;
                info.timezone_id = tz.timezone_id;
                info.name = tz.name;
                info.abbreviation = tz.abbreviation;
                info.offset = TimezoneOffset(tz.std_offset_minutes, false);
                info.observes_dst = tz.observes_dst;
                info.dst_start_month = tz.dst_start_month;
                info.dst_start_week = tz.dst_start_week;
                info.dst_start_day = tz.dst_start_day;
                info.dst_start_hour = tz.dst_start_hour;
                info.dst_end_month = tz.dst_end_month;
                info.dst_end_week = tz.dst_end_week;
                info.dst_end_day = tz.dst_end_day;
                info.dst_end_hour = tz.dst_end_hour;
                info.dst_offset_minutes = tz.dst_offset_minutes;

                timezones_[info.timezone_id] = info;
                if (!info.name.empty())
                {
                    name_to_id_[info.name] = info.timezone_id;
                }
                if (!info.abbreviation.empty())
                {
                    abbr_to_id_[info.abbreviation] = info.timezone_id;
                }
            }
            return Status::OK;
        }

        if (status == Status::NOT_FOUND || tz_list.empty())
        {
            initializeTimezones();
            return Status::OK;
        }

        return status;
    }

    auto TimezoneManager::getTimezoneInfo(uint16_t timezone_id) const -> const TimezoneInfo *
    {
        auto it = timezones_.find(timezone_id);
        return it != timezones_.end() ? &it->second : nullptr;
    }

    auto TimezoneManager::getTimezoneByName(const std::string &name) const -> uint16_t
    {
        auto it = name_to_id_.find(name);
        return it != name_to_id_.end() ? it->second : 1; // Default to UTC
    }

    auto TimezoneManager::getTimezoneByAbbreviation(const std::string &abbr) const -> uint16_t
    {
        auto it = abbr_to_id_.find(abbr);
        return it != abbr_to_id_.end() ? it->second : 1; // Default to UTC
    }

    static bool isLeapYear(int year)
    {
        return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    }

    static int daysInMonth(int year, int month)
    {
        static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month == 2)
        {
            return isLeapYear(year) ? 29 : 28;
        }
        if (month < 1 || month > 12)
        {
            return 30;
        }
        return kDays[month - 1];
    }

    // 0=Sunday, 1=Monday, ... 6=Saturday
    static int dayOfWeek(int year, int month, int day)
    {
        static const int kMonthOffsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (month < 3)
        {
            year -= 1;
        }
        return (year + year / 4 - year / 100 + year / 400 +
                kMonthOffsets[month - 1] + day) % 7;
    }

    static int nthWeekdayOfMonth(int year, int month, int weekday, int week)
    {
        if (week <= 0)
        {
            week = 5;
        }
        int first_dow = dayOfWeek(year, month, 1);
        int first_target = 1 + (7 + weekday - first_dow) % 7;
        if (week >= 5)
        {
            int last_day = daysInMonth(year, month);
            int last_dow = dayOfWeek(year, month, last_day);
            return last_day - (7 + last_dow - weekday) % 7;
        }
        return first_target + (week - 1) * 7;
    }

    static bool isWithinDSTByRule(const TimezoneInfo &info, int year, int month, int day, int hour)
    {
        if (!info.observes_dst || info.dst_start_month == 0 || info.dst_end_month == 0)
        {
            return false;
        }

        int start_dow = info.dst_start_day;
        int end_dow = info.dst_end_day;
        int start_day = nthWeekdayOfMonth(year, info.dst_start_month, start_dow, info.dst_start_week);
        int end_day = nthWeekdayOfMonth(year, info.dst_end_month, end_dow, info.dst_end_week);

        auto after_start = [&]() -> bool
        {
            if (month > info.dst_start_month) return true;
            if (month < info.dst_start_month) return false;
            if (day > start_day) return true;
            if (day < start_day) return false;
            return hour >= info.dst_start_hour;
        };

        auto before_end = [&]() -> bool
        {
            if (month < info.dst_end_month) return true;
            if (month > info.dst_end_month) return false;
            if (day < end_day) return true;
            if (day > end_day) return false;
            return hour < info.dst_end_hour;
        };

        if (info.dst_start_month < info.dst_end_month)
        {
            return after_start() && before_end();
        }
        if (info.dst_start_month > info.dst_end_month)
        {
            return after_start() || before_end();
        }
        if (start_day == end_day)
        {
            return hour >= info.dst_start_hour && hour < info.dst_end_hour;
        }
        return after_start() && before_end();
    }

    // Helper function: Check if a given time falls within DST for US timezones
    // US DST rules (since 2007):
    // - Start: Second Sunday in March at 2:00 AM local standard time
    // - End: First Sunday in November at 2:00 AM local daylight time
    static bool isWithinDST_US(int year, int month, int day, int hour)
    {
        // DST only applies March through November
        if (month < 3 || month > 11)
        {
            return false; // January, February, December are always standard time
        }

        if (month > 3 && month < 11)
        {
            return true; // April through October are always DST
        }

        // Calculate second Sunday in March
        // Start DST: Second Sunday in March at 2:00 AM
        if (month == 3)
        {
            // Find first day of March
            // Calculate what day of week March 1st falls on
            // Using Zeller's congruence for Gregorian calendar
            int march_1_dow = (1 + (13 * 1) / 5 + year + year / 4 - year / 100 + year / 400) % 7;
            // Adjust: 0=Saturday, 1=Sunday, 2=Monday, ..., 6=Friday

            // Days until first Sunday (0 if March 1 is Sunday)
            int days_to_first_sunday = (7 - march_1_dow + 1) % 7;
            if (days_to_first_sunday == 0)
                days_to_first_sunday =
                    7; // If March 1 is Sunday, first Sunday is March 1, second is March 8

            // Second Sunday is first Sunday + 7 days
            int second_sunday = days_to_first_sunday == 0 ? 8 : days_to_first_sunday + 7;

            if (day < second_sunday)
            {
                return false; // Before DST start
            }
            else if (day > second_sunday)
            {
                return true; // After DST start
            }
            else // day == second_sunday
            {
                return hour >= 2; // DST starts at 2:00 AM
            }
        }

        // Calculate first Sunday in November
        // End DST: First Sunday in November at 2:00 AM
        if (month == 11)
        {
            // Find first day of November
            int nov_1_dow = (1 + (13 * 9) / 5 + year + year / 4 - year / 100 + year / 400) % 7;

            // Days until first Sunday
            int days_to_first_sunday = (7 - nov_1_dow + 1) % 7;
            if (days_to_first_sunday == 0)
                days_to_first_sunday = 7;
            int first_sunday = days_to_first_sunday;

            if (day < first_sunday)
            {
                return true; // Before DST end, still in DST
            }
            else if (day > first_sunday)
            {
                return false; // After DST end
            }
            else // day == first_sunday
            {
                return hour < 2; // DST ends at 2:00 AM
            }
        }

        return false; // Fallback
    }

    auto TimezoneManager::getOffset(uint16_t timezone_id, int64_t gmt_microseconds) const
        -> TimezoneOffset
    {
        const auto *info = getTimezoneInfo(timezone_id);
        if (!info)
        {
            return TimezoneOffset(0, false); // Default to UTC
        }

        // If timezone doesn't observe DST, return standard offset
        if (!info->observes_dst)
        {
            return info->offset;
        }

        // Convert GMT timestamp to local time components for DST calculation
        // First, convert microseconds to seconds
        int64_t seconds = gmt_microseconds / 1000000;

        // Add standard offset to get local time
        int64_t local_seconds = seconds + (info->offset.offset_minutes * 60);

        // Convert to date components (simplified epoch-based calculation)
        // Unix epoch: 1970-01-01 00:00:00 UTC
        int64_t days_since_epoch = local_seconds / 86400;
        int64_t seconds_today = local_seconds % 86400;
        if (seconds_today < 0)
        {
            days_since_epoch--;
            seconds_today += 86400;
        }

        // Calculate year, month, day from days since epoch
        // This is a simplified version - good enough for DST calculation
        int year = 1970;
        int64_t remaining_days = days_since_epoch;

        // Skip to approximate year
        int years_elapsed = static_cast<int>(remaining_days / 365);
        year += years_elapsed;
        remaining_days -= years_elapsed * 365;
        remaining_days -= years_elapsed / 4; // Leap years

        // Refine year
        while (remaining_days < 0)
        {
            year--;
            remaining_days +=
                365 + ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0);
        }

        while (remaining_days >=
               365 + ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0))
        {
            remaining_days -=
                365 + ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0);
            year++;
        }

        // Calculate month and day
        int month = 1;
        int day = 1;
        for (int m = 0; m < 12; m++)
        {
            int dim = daysInMonth(year, m + 1);
            if (remaining_days < dim)
            {
                month = m + 1;
                day = static_cast<int>(remaining_days) + 1;
                break;
            }
            remaining_days -= dim;
        }

        int hour = static_cast<int>(seconds_today / 3600);

        bool in_dst = false;
        if (info->dst_start_month != 0 && info->dst_end_month != 0)
        {
            in_dst = isWithinDSTByRule(*info, year, month, day, hour);
        }
        else
        {
            // Fallback to US DST rules when no explicit rule is stored.
            in_dst = isWithinDST_US(year, month, day, hour);
        }

        if (in_dst)
        {
            int32_t dst_offset = info->dst_offset_minutes != 0 ? info->dst_offset_minutes : 60;
            return TimezoneOffset(info->offset.offset_minutes + dst_offset, true);
        }
        else
        {
            return info->offset;
        }
    }

    auto TimezoneManager::toGMT(int64_t local_microseconds, uint16_t from_timezone,
                                ErrorContext *ctx) const -> std::optional<int64_t>
    {
        TimezoneOffset offset = getOffset(from_timezone, local_microseconds);

        // Convert local time to GMT by subtracting the offset
        // Example: 10:00 EST (offset -5h) -> 15:00 GMT
        int64_t offset_microseconds = static_cast<int64_t>(offset.offset_minutes) * 60 * 1000000;
        return local_microseconds - offset_microseconds;
    }

    auto TimezoneManager::fromGMT(int64_t gmt_microseconds, uint16_t to_timezone,
                                  ErrorContext *ctx) const -> std::optional<int64_t>
    {
        TimezoneOffset offset = getOffset(to_timezone, gmt_microseconds);

        // Convert GMT to local time by adding the offset
        // Example: 15:00 GMT -> 10:00 EST (offset -5h)
        int64_t offset_microseconds = static_cast<int64_t>(offset.offset_minutes) * 60 * 1000000;
        return gmt_microseconds + offset_microseconds;
    }

    auto TimezoneManager::parseISO8601(const std::string &str, ErrorContext *ctx) const
        -> std::optional<std::pair<int64_t, std::optional<TimezoneOffset>>>
    {
        // Parse format: YYYY-MM-DD HH:MM:SS[.ffffff][+/-HH:MM]
        int year = 0, month = 0, day = 0;
        int hour = 0, minute = 0, second = 0;
        int microseconds = 0;

        // Find date/time separator (space or 'T')
        size_t sep_pos = str.find(' ');
        if (sep_pos == std::string::npos)
        {
            sep_pos = str.find('T');
        }

        if (sep_pos == std::string::npos)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid timestamp format: missing time component");
            }
            return std::nullopt;
        }

        // Parse date part
        std::string date_part = str.substr(0, sep_pos);
        if (sscanf(date_part.c_str(), "%d-%d-%d", &year, &month, &day) != 3)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid date format");
            }
            return std::nullopt;
        }

        // Parse time part (after separator)
        std::string time_part = str.substr(sep_pos + 1);

        // Look for timezone offset or name at the end
        std::optional<TimezoneOffset> tz_offset;
        size_t tz_pos = std::string::npos;

        // Check for +/- offset
        size_t plus_pos = time_part.find_last_of('+');
        size_t minus_pos = time_part.find_last_of('-');
        if (plus_pos != std::string::npos)
        {
            tz_pos = plus_pos;
        }
        else if (minus_pos != std::string::npos && minus_pos > 2) // Not negative time
        {
            tz_pos = minus_pos;
        }

        // Check for named timezone (UTC, EST, etc.)
        if (tz_pos == std::string::npos)
        {
            for (const auto &pair : abbr_to_id_)
            {
                if (time_part.length() >= pair.first.length() &&
                    time_part.substr(time_part.length() - pair.first.length()) == pair.first)
                {
                    const auto *info = getTimezoneInfo(pair.second);
                    if (info)
                    {
                        tz_offset = info->offset;
                        time_part = time_part.substr(0, time_part.length() - pair.first.length());
                        // Trim trailing space
                        while (!time_part.empty() && time_part.back() == ' ')
                        {
                            time_part.pop_back();
                        }
                        break;
                    }
                }
            }
        }

        if (tz_pos != std::string::npos)
        {
            std::string offset_str = time_part.substr(tz_pos);
            tz_offset = TimezoneOffset::fromString(offset_str, ctx);
            if (!tz_offset)
            {
                return std::nullopt;
            }
            time_part = time_part.substr(0, tz_pos);
        }

        // Parse time component
        size_t dot_pos = time_part.find('.');
        if (dot_pos != std::string::npos)
        {
            // Has fractional seconds
            std::string time_base = time_part.substr(0, dot_pos);
            std::string frac_str = time_part.substr(dot_pos + 1);

            if (sscanf(time_base.c_str(), "%d:%d:%d", &hour, &minute, &second) != 3)
            {
                if (ctx)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid time format");
                }
                return std::nullopt;
            }

            // Parse fractional seconds (up to 6 digits for microseconds)
            if (frac_str.length() > 6)
            {
                frac_str = frac_str.substr(0, 6);
            }
            while (frac_str.length() < 6)
            {
                frac_str += '0';
            }
            microseconds = std::stoi(frac_str);
        }
        else
        {
            if (sscanf(time_part.c_str(), "%d:%d:%d", &hour, &minute, &second) != 3)
            {
                if (ctx)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid time format");
                }
                return std::nullopt;
            }
        }

        // Validate ranges
        // Allow second = 60 for leap seconds (ISO 8601)
        if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 ||
            minute > 59 || second < 0 || second > 60 || microseconds < 0 || microseconds > 999999)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Date/time component out of range");
            }
            return std::nullopt;
        }

        // Convert to microseconds since epoch (simplified calculation)
        // For production, use proper calendar math library
        struct tm timeinfo = {};
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = second;
        timeinfo.tm_isdst = -1;

        time_t epoch_seconds = timegmPortable(&timeinfo);
        if (epoch_seconds == -1)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to convert to timestamp");
            }
            return std::nullopt;
        }

        int64_t result = static_cast<int64_t>(epoch_seconds) * 1000000 + microseconds;
        return std::make_pair(result, tz_offset);
    }

    auto TimezoneManager::parseTimestamp(const std::string &str, uint16_t default_tz,
                                         ErrorContext *ctx) const -> std::optional<int64_t>
    {
        auto result = parseISO8601(str, ctx);
        if (!result)
        {
            return std::nullopt;
        }

        int64_t local_time = result->first;
        std::optional<TimezoneOffset> tz_offset = result->second;

        if (tz_offset)
        {
            // Has explicit timezone - convert to GMT
            int64_t offset_microseconds =
                static_cast<int64_t>(tz_offset->offset_minutes) * 60 * 1000000;
            return local_time - offset_microseconds;
        }
        else
        {
            // No timezone specified - use default
            return toGMT(local_time, default_tz, ctx);
        }
    }

    auto TimezoneManager::formatTimestamp(int64_t gmt_microseconds, uint16_t display_tz,
                                          bool show_offset) const -> std::string
    {
        // Convert GMT to local time in display timezone
        auto local_opt = fromGMT(gmt_microseconds, display_tz, nullptr);
        if (!local_opt)
        {
            return "INVALID_TIMESTAMP";
        }

        int64_t local_microseconds = *local_opt;

        // Extract components
        int64_t total_seconds = local_microseconds / 1000000;
        int64_t microseconds = local_microseconds % 1000000;
        if (microseconds < 0)
        {
            microseconds += 1000000;
            total_seconds -= 1;
        }

        time_t epoch_seconds = static_cast<time_t>(total_seconds);
        struct tm timeinfo;
        if (!gmtimeUtc(epoch_seconds, &timeinfo))
        {
            std::memset(&timeinfo, 0, sizeof(timeinfo));
        }

        // Format: YYYY-MM-DD HH:MM:SS.ffffff
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(4) << (timeinfo.tm_year + 1900) << '-' << std::setw(2)
            << (timeinfo.tm_mon + 1) << '-' << std::setw(2) << timeinfo.tm_mday << ' '
            << std::setw(2) << timeinfo.tm_hour << ':' << std::setw(2) << timeinfo.tm_min << ':'
            << std::setw(2) << timeinfo.tm_sec << '.' << std::setw(6) << microseconds;

        if (show_offset)
        {
            TimezoneOffset offset = getOffset(display_tz, gmt_microseconds);
            oss << offset.toString();
        }

        return oss.str();
    }

    auto getThreadLocalTimezoneManager() -> TimezoneManager&
    {
        static thread_local TimezoneManager manager;
        return manager;
    }

} // namespace scratchbird::core
