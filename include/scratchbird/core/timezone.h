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

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <string>
#include <unordered_map>
#include <cstdint>
#include <optional>

namespace scratchbird::core
{
    class CatalogManager;

    /**
     * Timezone offset representation
     * All timestamps are stored internally in GMT/UTC
     * Offsets are used only for display and input parsing
     */
    struct TimezoneOffset
    {
        int32_t offset_minutes; // Offset from GMT in minutes (-720 to +840)
        bool is_dst;            // Whether this offset includes DST

        TimezoneOffset() : offset_minutes(0), is_dst(false) {}
        TimezoneOffset(int32_t minutes, bool dst = false) : offset_minutes(minutes), is_dst(dst) {}

        // Convert to string format (+/-HH:MM)
        std::string toString() const;

        // Parse from string format (+/-HH:MM or +/-HHMM)
        static auto fromString(const std::string &str, ErrorContext *ctx = nullptr)
            -> std::optional<TimezoneOffset>;
    };

    /**
     * Timezone information
     * Contains metadata about a timezone including its name and offset
     */
    struct TimezoneInfo
    {
        uint16_t timezone_id;     // Unique ID for this timezone
        std::string name;         // e.g., "America/New_York", "UTC", "GMT"
        std::string abbreviation; // e.g., "EST", "PST", "UTC"
        TimezoneOffset offset;    // Current offset from GMT
        bool observes_dst;        // Whether this timezone observes DST
        uint8_t dst_start_month = 0;
        uint8_t dst_start_week = 0;
        uint8_t dst_start_day = 0;
        uint8_t dst_start_hour = 0;
        uint8_t dst_end_month = 0;
        uint8_t dst_end_week = 0;
        uint8_t dst_end_day = 0;
        uint8_t dst_end_hour = 0;
        int32_t dst_offset_minutes = 0;

        TimezoneInfo()
            : timezone_id(0), name("UTC"), abbreviation("UTC"), offset(0, false),
              observes_dst(false)
        {
        }
    };

    /**
     * Timezone Manager
     *
     * Design principles:
     * 1. All TIMESTAMP values are stored internally as microseconds since epoch in GMT
     * 2. Column-level timezone hints specify how to display/interpret values
     * 3. Connection-level timezone controls default behavior
     * 4. All comparisons happen in GMT (no conversion needed)
     * 5. Conversion only happens at input (string->timestamp) and output (timestamp->string)
     */
    class TimezoneManager
    {
    public:
        TimezoneManager();
        ~TimezoneManager();

        // Load timezone definitions from catalog (falls back to built-ins if catalog is empty).
        auto loadFromCatalog(CatalogManager* catalog, ErrorContext* ctx = nullptr) -> Status;

        // Timezone lookup
        auto getTimezoneInfo(uint16_t timezone_id) const -> const TimezoneInfo *;
        auto getTimezoneByName(const std::string &name) const -> uint16_t;
        auto getTimezoneByAbbreviation(const std::string &abbr) const -> uint16_t;

        // Default timezone (UTC/GMT)
        auto getDefaultTimezone() const -> uint16_t
        {
            return 1;
        } // UTC

        // Common timezones
        static constexpr uint16_t TZ_GMT = 1;
        static constexpr uint16_t TZ_UTC = 1; // Same as GMT
        static constexpr uint16_t TZ_EST = 2;
        static constexpr uint16_t TZ_PST = 3;
        static constexpr uint16_t TZ_CST = 4;
        static constexpr uint16_t TZ_MST = 5;

        /**
         * Convert timestamp from one timezone to GMT
         * Input: local time in source timezone
         * Output: GMT time
         */
        auto toGMT(int64_t local_microseconds, uint16_t from_timezone,
                   ErrorContext *ctx = nullptr) const -> std::optional<int64_t>;

        /**
         * Convert timestamp from GMT to target timezone
         * Input: GMT time
         * Output: local time in target timezone
         */
        auto fromGMT(int64_t gmt_microseconds, uint16_t to_timezone,
                     ErrorContext *ctx = nullptr) const -> std::optional<int64_t>;

        /**
         * Parse timestamp string with optional timezone
         * Formats supported:
         *   - "2025-10-04 15:30:00"           -> uses default_tz
         *   - "2025-10-04 15:30:00+00:00"     -> explicit offset
         *   - "2025-10-04 15:30:00 UTC"       -> named timezone
         *   - "2025-10-04 15:30:00-05:00"     -> EST offset
         *
         * Returns: microseconds since epoch in GMT
         */
        auto parseTimestamp(const std::string &str, uint16_t default_tz,
                            ErrorContext *ctx = nullptr) const -> std::optional<int64_t>;

        /**
         * Format timestamp to string with timezone
         * Input: microseconds since epoch in GMT
         * Output: string in target timezone with offset indicator
         *
         * Examples:
         *   - formatTimestamp(ts, TZ_UTC) -> "2025-10-04 15:30:00+00:00"
         *   - formatTimestamp(ts, TZ_EST) -> "2025-10-04 10:30:00-05:00"
         */
        auto formatTimestamp(int64_t gmt_microseconds, uint16_t display_tz,
                             bool show_offset = true) const -> std::string;

        /**
         * Get offset for a timezone at a specific time
         * (Accounts for DST if applicable)
         */
        auto getOffset(uint16_t timezone_id, int64_t gmt_microseconds) const -> TimezoneOffset;

    private:
        std::unordered_map<uint16_t, TimezoneInfo> timezones_;
        std::unordered_map<std::string, uint16_t> name_to_id_;
        std::unordered_map<std::string, uint16_t> abbr_to_id_;

        void initializeTimezones();

        // Helper: Parse ISO 8601 timestamp string
        auto parseISO8601(const std::string &str, ErrorContext *ctx = nullptr) const
            -> std::optional<std::pair<int64_t, std::optional<TimezoneOffset>>>;
    };

    // Thread-local timezone manager used by type conversions and expression evaluation.
    auto getThreadLocalTimezoneManager() -> TimezoneManager&;

} // namespace scratchbird::core
