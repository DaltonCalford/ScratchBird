#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace scratchbird::core
{
    /**
     * TypeExtractor - Utility class for extracting components from temporal and UUID types
     *
     * Provides static methods for extracting date/time components from int64 representations
     * and UUID version/variant information.
     */
    class TypeExtractor
    {
    public:
        // Date extraction (from days since Unix epoch)
        static int32_t extractYear(int64_t days);
        static int32_t extractMonth(int64_t days);
        static int32_t extractDay(int64_t days);
        static int32_t extractDayOfWeek(int64_t days);  // 0=Sunday, 6=Saturday
        static int32_t extractDayOfYear(int64_t days);  // 1-366
        static int32_t extractQuarter(int64_t days);    // 1-4

        // Time extraction (from microseconds)
        static int32_t extractHour(int64_t microseconds);
        static int32_t extractMinute(int64_t microseconds);
        static int32_t extractSecond(int64_t microseconds);
        static int32_t extractMillisecond(int64_t microseconds);
        static int32_t extractMicrosecond(int64_t microseconds);

        // Timestamp extraction (from microseconds since Unix epoch)
        static int32_t extractTimestampYear(int64_t microseconds);
        static int32_t extractTimestampMonth(int64_t microseconds);
        static int32_t extractTimestampDay(int64_t microseconds);
        static int32_t extractTimestampHour(int64_t microseconds);
        static int32_t extractTimestampMinute(int64_t microseconds);
        static int32_t extractTimestampSecond(int64_t microseconds);
        static int32_t extractTimestampMicrosecond(int64_t microseconds);

        // UUID extraction
        static int32_t extractUUIDVersion(const std::vector<uint8_t>& uuid);
        static int32_t extractUUIDVariant(const std::vector<uint8_t>& uuid);
        static std::optional<int64_t> extractUUIDTimestamp(const std::vector<uint8_t>& uuid, void* error_context);

    private:
        // Helper: Convert days since epoch to year/month/day
        static void daysToYMD(int64_t days, int32_t& year, int32_t& month, int32_t& day);
    };

} // namespace scratchbird::core
