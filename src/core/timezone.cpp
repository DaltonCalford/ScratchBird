#include "scratchbird/core/timezone.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <algorithm>

namespace scratchbird::core
{

    // ===== TimezoneOffset Implementation =====

    std::string TimezoneOffset::toString() const
    {
        int hours = offset_minutes / 60;
        int minutes = std::abs(offset_minutes % 60);

        std::ostringstream oss;
        oss << (offset_minutes >= 0 ? '+' : '-')
            << std::setfill('0') << std::setw(2) << std::abs(hours)
            << ':'
            << std::setfill('0') << std::setw(2) << minutes;
        return oss.str();
    }

    auto TimezoneOffset::fromString(const std::string &str, ErrorContext *ctx)
        -> std::optional<TimezoneOffset>
    {
        if (str.empty())
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Empty timezone offset string");
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

        if (hours < -12 || hours > 14 || minutes < 0 || minutes > 59)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Timezone offset out of range");
            }
            return std::nullopt;
        }

        int total_minutes = sign * (hours * 60 + minutes);
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
        // UTC/GMT (ID 1)
        TimezoneInfo utc;
        utc.timezone_id = 1;
        utc.name = "UTC";
        utc.abbreviation = "UTC";
        utc.offset = TimezoneOffset(0, false);
        utc.observes_dst = false;
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
        timezones_[5] = mst;
        name_to_id_["America/Denver"] = 5;
        name_to_id_["MST"] = 5;
        name_to_id_["MDT"] = 5;
        abbr_to_id_["MST"] = 5;
        abbr_to_id_["MDT"] = 5;
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

    auto TimezoneManager::getOffset(uint16_t timezone_id, int64_t gmt_microseconds) const
        -> TimezoneOffset
    {
        const auto *info = getTimezoneInfo(timezone_id);
        if (!info)
        {
            return TimezoneOffset(0, false); // Default to UTC
        }

        // For now, return the standard offset
        // Full DST support would require checking the date against DST rules
        // TODO(timezone): Implement full DST calculation based on date
        return info->offset;
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
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid date format");
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
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid time format");
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
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid time format");
                }
                return std::nullopt;
            }
        }

        // Validate ranges
        if (month < 1 || month > 12 || day < 1 || day > 31 ||
            hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
            second < 0 || second > 59 || microseconds < 0 || microseconds > 999999)
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

        time_t epoch_seconds = timegm(&timeinfo);
        if (epoch_seconds == -1)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Failed to convert to timestamp");
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
            int64_t offset_microseconds = static_cast<int64_t>(tz_offset->offset_minutes) * 60 * 1000000;
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
        gmtime_r(&epoch_seconds, &timeinfo);

        // Format: YYYY-MM-DD HH:MM:SS.ffffff
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(4) << (timeinfo.tm_year + 1900) << '-'
            << std::setw(2) << (timeinfo.tm_mon + 1) << '-'
            << std::setw(2) << timeinfo.tm_mday << ' '
            << std::setw(2) << timeinfo.tm_hour << ':'
            << std::setw(2) << timeinfo.tm_min << ':'
            << std::setw(2) << timeinfo.tm_sec << '.'
            << std::setw(6) << microseconds;

        if (show_offset)
        {
            TimezoneOffset offset = getOffset(display_tz, gmt_microseconds);
            oss << offset.toString();
        }

        return oss.str();
    }

} // namespace scratchbird::core
