#include "scratchbird/core/type_extractor.h"
#include <ctime>
#include <cstdlib>

namespace scratchbird::core
{
    // Helper: Convert days since Unix epoch (1970-01-01) to year/month/day
    void TypeExtractor::daysToYMD(int64_t days, int32_t& year, int32_t& month, int32_t& day)
    {
        // Simple Gregorian calendar calculation
        // Days since 1970-01-01
        const int64_t DAYS_PER_400_YEARS = 146097;
        const int64_t DAYS_PER_100_YEARS = 36524;
        const int64_t DAYS_PER_4_YEARS = 1461;
        const int64_t DAYS_PER_YEAR = 365;

        // Adjust to days since year 0 (add days from year 0 to 1970)
        int64_t d = days + 719468;

        int64_t era = (d >= 0 ? d : d - DAYS_PER_400_YEARS + 1) / DAYS_PER_400_YEARS;
        int64_t doe = d - era * DAYS_PER_400_YEARS;
        int64_t yoe = (doe - doe / DAYS_PER_4_YEARS + doe / DAYS_PER_100_YEARS) / DAYS_PER_YEAR;
        int64_t y = yoe + era * 400;
        int64_t doy = doe - (DAYS_PER_YEAR * yoe + yoe / 4 - yoe / 100);

        int64_t mp = (5 * doy + 2) / 153;
        int64_t d_day = doy - (153 * mp + 2) / 5 + 1;
        int64_t m = mp + (mp < 10 ? 3 : -9);

        year = static_cast<int32_t>(y + (m <= 2));
        month = static_cast<int32_t>(m);
        day = static_cast<int32_t>(d_day);
    }

    // Date extraction
    int32_t TypeExtractor::extractYear(int64_t days)
    {
        int32_t year, month, day;
        daysToYMD(days, year, month, day);
        return year;
    }

    int32_t TypeExtractor::extractMonth(int64_t days)
    {
        int32_t year, month, day;
        daysToYMD(days, year, month, day);
        return month;
    }

    int32_t TypeExtractor::extractDay(int64_t days)
    {
        int32_t year, month, day;
        daysToYMD(days, year, month, day);
        return day;
    }

    int32_t TypeExtractor::extractDayOfWeek(int64_t days)
    {
        // Unix epoch (1970-01-01) was a Thursday (4)
        // 0=Sunday, 1=Monday, ..., 6=Saturday
        int64_t dow = (days + 4) % 7;
        if (dow < 0) dow += 7;
        return static_cast<int32_t>(dow);
    }

    int32_t TypeExtractor::extractDayOfYear(int64_t days)
    {
        int32_t year, month, day;
        daysToYMD(days, year, month, day);

        // Calculate day of year
        const int days_before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        int32_t doy = days_before_month[month - 1] + day;

        // Add leap day if after February in a leap year
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (is_leap && month > 2) {
            doy++;
        }

        return doy;
    }

    int32_t TypeExtractor::extractQuarter(int64_t days)
    {
        int32_t month = extractMonth(days);
        return (month - 1) / 3 + 1;
    }

    bool TypeExtractor::isLeapYear(int32_t year)
    {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    int64_t TypeExtractor::ymdToDays(int32_t year, int32_t month, int32_t day)
    {
        // Howard Hinnant's days_from_civil algorithm.
        year -= (month <= 2);
        const int64_t era = (year >= 0 ? year : year - 399) / 400;
        const uint32_t yoe = static_cast<uint32_t>(year - era * 400);
        const uint32_t doy =
            (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + static_cast<uint32_t>(day) - 1;
        const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
    }

    int32_t TypeExtractor::extractWeek(int64_t days)
    {
        int32_t year = 0;
        int32_t month = 0;
        int32_t day = 0;
        daysToYMD(days, year, month, day);
        int32_t jan1_dow = extractDayOfWeek(ymdToDays(year, 1, 1)); // 0=Sunday
        int64_t first_sunday = ymdToDays(year, 1, 1) - jan1_dow;
        int64_t week = (days - first_sunday) / 7 + 1;
        return static_cast<int32_t>(week);
    }

    int32_t TypeExtractor::extractISODayOfWeek(int64_t days)
    {
        int32_t dow = extractDayOfWeek(days); // 0=Sunday
        return (dow == 0) ? 7 : dow;
    }

    int32_t TypeExtractor::isoWeeksInYear(int32_t year)
    {
        int64_t dec31_days = ymdToDays(year, 12, 31);
        int32_t dow = extractDayOfWeek(dec31_days); // 0=Sunday
        return (dow == 4 || (isLeapYear(year) && dow == 5)) ? 53 : 52;
    }

    int32_t TypeExtractor::extractISOWeek(int64_t days)
    {
        int32_t year = 0;
        int32_t month = 0;
        int32_t day = 0;
        daysToYMD(days, year, month, day);
        int32_t doy = extractDayOfYear(days);
        int32_t dow = extractISODayOfWeek(days); // 1=Mon..7=Sun
        int32_t week = (doy - dow + 10) / 7;
        if (week < 1) {
            return isoWeeksInYear(year - 1);
        }
        int32_t weeks_in_year = isoWeeksInYear(year);
        if (week > weeks_in_year) {
            return 1;
        }
        return week;
    }

    int32_t TypeExtractor::extractISOYear(int64_t days)
    {
        int32_t year = 0;
        int32_t month = 0;
        int32_t day = 0;
        daysToYMD(days, year, month, day);
        int32_t doy = extractDayOfYear(days);
        int32_t dow = extractISODayOfWeek(days); // 1=Mon..7=Sun
        int32_t week = (doy - dow + 10) / 7;
        if (week < 1) {
            return year - 1;
        }
        int32_t weeks_in_year = isoWeeksInYear(year);
        if (week > weeks_in_year) {
            return year + 1;
        }
        return year;
    }

    int32_t TypeExtractor::extractCentury(int64_t days)
    {
        int32_t year = 0;
        int32_t month = 0;
        int32_t day = 0;
        daysToYMD(days, year, month, day);
        if (year > 0) {
            return (year + 99) / 100;
        }
        return -((std::abs(year) + 99) / 100);
    }

    int32_t TypeExtractor::extractDecade(int64_t days)
    {
        int32_t year = 0;
        int32_t month = 0;
        int32_t day = 0;
        daysToYMD(days, year, month, day);
        if (year >= 0) {
            return year / 10;
        }
        return -((std::abs(year) + 9) / 10);
    }

    int32_t TypeExtractor::extractMillennium(int64_t days)
    {
        int32_t year = 0;
        int32_t month = 0;
        int32_t day = 0;
        daysToYMD(days, year, month, day);
        if (year > 0) {
            return (year + 999) / 1000;
        }
        return -((std::abs(year) + 999) / 1000);
    }

    // Time extraction
    int32_t TypeExtractor::extractHour(int64_t microseconds)
    {
        int64_t total_seconds = microseconds / 1000000;
        int64_t hours = (total_seconds / 3600) % 24;
        return static_cast<int32_t>(hours);
    }

    int32_t TypeExtractor::extractMinute(int64_t microseconds)
    {
        int64_t total_seconds = microseconds / 1000000;
        int64_t minutes = (total_seconds / 60) % 60;
        return static_cast<int32_t>(minutes);
    }

    int32_t TypeExtractor::extractSecond(int64_t microseconds)
    {
        int64_t total_seconds = microseconds / 1000000;
        int64_t seconds = total_seconds % 60;
        return static_cast<int32_t>(seconds);
    }

    int32_t TypeExtractor::extractMillisecond(int64_t microseconds)
    {
        int64_t milliseconds = (microseconds / 1000) % 1000;
        return static_cast<int32_t>(milliseconds);
    }

    int32_t TypeExtractor::extractMicrosecond(int64_t microseconds)
    {
        int64_t micros = microseconds % 1000000;
        return static_cast<int32_t>(micros);
    }

    // Timestamp extraction
    int32_t TypeExtractor::extractTimestampYear(int64_t microseconds)
    {
        int64_t days = microseconds / (1000000LL * 86400LL);
        return extractYear(days);
    }

    int32_t TypeExtractor::extractTimestampMonth(int64_t microseconds)
    {
        int64_t days = microseconds / (1000000LL * 86400LL);
        return extractMonth(days);
    }

    int32_t TypeExtractor::extractTimestampDay(int64_t microseconds)
    {
        int64_t days = microseconds / (1000000LL * 86400LL);
        return extractDay(days);
    }

    int32_t TypeExtractor::extractTimestampHour(int64_t microseconds)
    {
        return extractHour(microseconds);
    }

    int32_t TypeExtractor::extractTimestampMinute(int64_t microseconds)
    {
        return extractMinute(microseconds);
    }

    int32_t TypeExtractor::extractTimestampSecond(int64_t microseconds)
    {
        return extractSecond(microseconds);
    }

    int32_t TypeExtractor::extractTimestampMicrosecond(int64_t microseconds)
    {
        return extractMicrosecond(microseconds);
    }

    // UUID extraction
    int32_t TypeExtractor::extractUUIDVersion(const std::vector<uint8_t>& uuid)
    {
        if (uuid.size() < 16) return 0;
        // Version is in the high nibble of byte 6
        return (uuid[6] >> 4) & 0x0F;
    }

    int32_t TypeExtractor::extractUUIDVariant(const std::vector<uint8_t>& uuid)
    {
        if (uuid.size() < 16) return 0;
        // Variant is in the high bits of byte 8
        uint8_t variant_byte = uuid[8];
        if ((variant_byte & 0x80) == 0) return 0;      // NCS
        if ((variant_byte & 0xC0) == 0x80) return 1;   // RFC 4122
        if ((variant_byte & 0xE0) == 0xC0) return 2;   // Microsoft
        return 3;                                       // Reserved
    }

    std::optional<int64_t> TypeExtractor::extractUUIDTimestamp(const std::vector<uint8_t>& uuid, void* error_context)
    {
        if (uuid.size() < 16) return std::nullopt;

        int32_t version = extractUUIDVersion(uuid);
        if (version != 1 && version != 7) {
            // Only UUID v1 and v7 have timestamps
            return std::nullopt;
        }

        if (version == 1) {
            // UUID v1: 60-bit timestamp in 100-nanosecond intervals since 1582-10-15
            uint64_t time_low = (uint64_t(uuid[0]) << 24) | (uint64_t(uuid[1]) << 16) |
                                (uint64_t(uuid[2]) << 8) | uint64_t(uuid[3]);
            uint64_t time_mid = (uint64_t(uuid[4]) << 8) | uint64_t(uuid[5]);
            uint64_t time_hi = uint64_t(uuid[6] & 0x0F) << 8 | uint64_t(uuid[7]);

            uint64_t timestamp_100ns = (time_hi << 48) | (time_mid << 32) | time_low;

            // Convert to microseconds since Unix epoch
            // UUID epoch is 1582-10-15, Unix epoch is 1970-01-01
            // Difference: 12219292800 seconds = 122192928000000000 * 100ns
            const uint64_t UUID_TO_UNIX_100NS = 122192928000000000ULL;
            if (timestamp_100ns < UUID_TO_UNIX_100NS) return std::nullopt;

            uint64_t unix_100ns = timestamp_100ns - UUID_TO_UNIX_100NS;
            return static_cast<int64_t>(unix_100ns / 10);  // Convert to microseconds
        } else {
            // UUID v7: 48-bit Unix timestamp in milliseconds
            uint64_t timestamp_ms = (uint64_t(uuid[0]) << 40) | (uint64_t(uuid[1]) << 32) |
                                    (uint64_t(uuid[2]) << 24) | (uint64_t(uuid[3]) << 16) |
                                    (uint64_t(uuid[4]) << 8) | uint64_t(uuid[5]);
            return static_cast<int64_t>(timestamp_ms * 1000);  // Convert to microseconds
        }
    }

} // namespace scratchbird::core
