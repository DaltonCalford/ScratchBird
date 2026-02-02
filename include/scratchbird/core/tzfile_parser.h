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
#include <vector>
#include <cstdint>

namespace scratchbird::core
{

    /**
     * Leap second record from TZif file
     */
    struct LeapSecond
    {
        int64_t time;  // When leap second occurs (Unix timestamp)
        int32_t total; // Total leap seconds after this point
    };

    /**
     * Single timezone transition
     * Represents a point in time when the timezone rules change
     */
    struct TimezoneTransition
    {
        int64_t timestamp;        // Unix timestamp when transition occurs
        int32_t utc_offset;       // Seconds from UTC at this time
        bool is_dst;              // Is daylight saving time in effect?
        std::string abbreviation; // Timezone abbreviation (e.g., "EST", "EDT")
    };

    /**
     * Complete timezone information parsed from TZif file
     */
    struct TimezoneData
    {
        std::string name;                             // Timezone name (e.g., "America/New_York")
        std::vector<TimezoneTransition> transitions;  // Historical and future transitions
        std::string posix_tz_string;                  // POSIX TZ string for future dates
        std::vector<LeapSecond> leap_seconds;         // Leap second records

        TimezoneData() = default;
        TimezoneData(const std::string &tz_name) : name(tz_name) {}
    };

    /**
     * TZif File Parser
     *
     * Parses IANA timezone database files in TZif (Time Zone Information Format) binary format.
     * Supports both TZif version 2 and version 3 files.
     *
     * TZif Format:
     * - Header with magic number "TZif" and version
     * - Transition times (when timezone rules change)
     * - Transition types (UTC offset, DST flag, abbreviation)
     * - Leap second records
     * - POSIX TZ string for future dates
     *
     * Usage:
     *   TZFileParser parser;
     *   TimezoneData tz_data;
     *   ErrorContext ctx;
     *   Status status = parser.parseFile("/usr/share/zoneinfo/America/New_York", tz_data, &ctx);
     */
    class TZFileParser
    {
    public:
        TZFileParser() = default;
        ~TZFileParser() = default;

        /**
         * Parse a single TZif binary file
         *
         * @param filepath Full path to TZif file (e.g., "/usr/share/zoneinfo/America/New_York")
         * @param tz_data Output timezone data structure
         * @param ctx Error context for detailed error reporting
         * @return Status::OK on success, error status otherwise
         */
        auto parseFile(const std::string &filepath,
                       TimezoneData &tz_data,
                       ErrorContext *ctx = nullptr) -> Status;

        /**
         * Parse entire timezone directory tree
         *
         * Recursively scans a zoneinfo directory and parses all timezone files.
         * Supports both compiled binary TZif files and IANA source files.
         *
         * @param zoneinfo_dir Path to zoneinfo directory (e.g., "/usr/share/zoneinfo" or "resources/timezones")
         * @param timezones Output vector of parsed timezone data
         * @param ctx Error context for detailed error reporting
         * @return Status::OK on success, error status otherwise
         */
        auto parseDirectory(const std::string &zoneinfo_dir,
                            std::vector<TimezoneData> &timezones,
                            ErrorContext *ctx = nullptr) -> Status;

    private:
        // TZif file format structures
        struct TZifHeader
        {
            char magic[4];        // "TZif"
            uint8_t version;      // '2', '3', or '\0' for version 1
            char reserved[15];    // Reserved for future use
            uint32_t ttisgmtcnt;  // Number of UTC/local indicators
            uint32_t ttisstdcnt;  // Number of standard/wall indicators
            uint32_t leapcnt;     // Number of leap second records
            uint32_t timecnt;     // Number of transition times
            uint32_t typecnt;     // Number of transition types
            uint32_t charcnt;     // Number of timezone abbreviation chars
        };

        struct TransitionTime
        {
            int64_t time;       // Unix timestamp of transition
            uint8_t type_index; // Index into transition types array
        };

        struct TransitionType
        {
            int32_t utoff;    // UTC offset in seconds
            uint8_t isdst;    // Is DST in effect (0 or 1)
            uint8_t abbrind;  // Index into abbreviation string
        };

        // Parsing helper methods
        auto readHeader(FILE *fp, TZifHeader &header, ErrorContext *ctx) -> Status;
        auto readTransitions32(FILE *fp, const TZifHeader &header,
                               std::vector<TransitionTime> &transitions,
                               ErrorContext *ctx) -> Status;
        auto readTransitions64(FILE *fp, const TZifHeader &header,
                               std::vector<TransitionTime> &transitions,
                               ErrorContext *ctx) -> Status;
        auto readTypes(FILE *fp, const TZifHeader &header,
                       std::vector<TransitionType> &types,
                       ErrorContext *ctx) -> Status;
        auto readAbbreviations(FILE *fp, const TZifHeader &header,
                               std::string &abbrevs,
                               ErrorContext *ctx) -> Status;
        auto readLeapSeconds32(FILE *fp, const TZifHeader &header,
                               std::vector<LeapSecond> &leaps,
                               ErrorContext *ctx) -> Status;
        auto readLeapSeconds64(FILE *fp, const TZifHeader &header,
                               std::vector<LeapSecond> &leaps,
                               ErrorContext *ctx) -> Status;
        auto readPosixTZ(FILE *fp, std::string &posix_tz, ErrorContext *ctx) -> Status;

        // Binary reading utilities
        auto read8(FILE *fp) -> uint8_t;
        auto read32(FILE *fp) -> uint32_t;
        auto read64(FILE *fp) -> uint64_t;

        // Extract timezone name from file path
        auto extractTimezoneName(const std::string &filepath) -> std::string;

        // Recursively scan directory for timezone files
        auto scanDirectory(const std::string &dir_path,
                           std::vector<std::string> &files,
                           ErrorContext *ctx) -> Status;
    };

} // namespace scratchbird::core
