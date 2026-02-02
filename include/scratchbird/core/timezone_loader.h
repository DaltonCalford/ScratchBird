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
#include "scratchbird/core/tzfile_parser.h"
#include "scratchbird/core/catalog_manager.h"
#include <string>
#include <vector>
#include <cstdint>

namespace scratchbird::core
{

    /**
     * Timezone Loader
     *
     * Loads timezone data from IANA TZif files into the ScratchBird catalog.
     * Bridges the TZFileParser (which reads IANA timezone data) and the
     * CatalogManager (which stores timezone information).
     *
     * Conversion Strategy:
     * - Full timezone transition data is analyzed to extract simplified DST rules
     * - Most recent transition pattern is used to determine DST start/end rules
     * - Timezones without transitions use fixed offset
     *
     * Usage:
     *   Database* db = Database::open("mydb.sb");
     *   TimezoneLoader loader(db->getCatalog());
     *   ErrorContext ctx;
     *   loader.loadFromDirectory("/usr/share/zoneinfo", &ctx);
     */
    class TimezoneLoader
    {
    public:
        /**
         * Constructor
         *
         * @param catalog Pointer to the catalog manager where timezone data will be stored
         */
        explicit TimezoneLoader(CatalogManager *catalog);

        ~TimezoneLoader() = default;

        /**
         * Load a single timezone into the catalog
         *
         * Converts full timezone transition data into simplified DST rules
         * and stores in the catalog using CatalogManager::createTimezone.
         *
         * @param tz_data Timezone data parsed from TZif file
         * @param ctx Error context for detailed error reporting
         * @return Status::OK on success, error status otherwise
         */
        auto loadTimezone(const TimezoneData &tz_data, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Load all timezones from a directory
         *
         * Scans a zoneinfo directory (e.g., /usr/share/zoneinfo), parses all
         * TZif files, and loads them into the catalog.
         *
         * @param zoneinfo_dir Path to zoneinfo directory
         * @param ctx Error context for detailed error reporting
         * @return Status::OK on success, error status otherwise
         *
         * Note: Failures on individual timezone files are logged but do not
         * stop the loading process. The method returns OK if at least some
         * timezones were loaded successfully.
         */
        auto loadFromDirectory(const std::string &zoneinfo_dir,
                               ErrorContext *ctx = nullptr) -> Status;

        /**
         * Load a single timezone file
         *
         * Parses a TZif file and loads it into the catalog.
         *
         * @param filepath Full path to TZif file (e.g., "/usr/share/zoneinfo/America/New_York")
         * @param ctx Error context for detailed error reporting
         * @return Status::OK on success, error status otherwise
         */
        auto loadFromFile(const std::string &filepath,
                          ErrorContext *ctx = nullptr) -> Status;

        /**
         * Clear all timezone data from catalog
         *
         * WARNING: This removes all timezone records. Use with caution.
         *
         * @param ctx Error context for detailed error reporting
         * @return Status::OK on success, error status otherwise
         */
        auto clearAllTimezones(ErrorContext *ctx = nullptr) -> Status;

        /**
         * Get statistics about loaded timezones
         *
         * @param total_count Output: total number of timezones loaded
         * @param with_dst_count Output: number of timezones with DST
         * @param ctx Error context for detailed error reporting
         * @return Status::OK on success, error status otherwise
         */
        auto getLoadedTimezoneStats(size_t &total_count,
                                    size_t &with_dst_count,
                                    ErrorContext *ctx = nullptr) -> Status;

    private:
        CatalogManager *catalog_;
        TZFileParser parser_;

        // Next available timezone ID (starts at 100 to avoid conflicts with built-in timezones)
        uint16_t next_timezone_id_ = 100;

        /**
         * Convert TimezoneData (full transition data) to TimezoneInfo (simplified DST rules)
         *
         * Analyzes the transition data to extract:
         * - Standard offset (most common non-DST offset)
         * - DST start rule (month, week, day, hour)
         * - DST end rule (month, week, day, hour)
         * - DST offset
         *
         * @param tz_data Full timezone data from TZif file
         * @param tz_info Output: simplified timezone info for catalog
         */
        void convertToSimplifiedDST(const TimezoneData &tz_data,
                                    CatalogManager::TimezoneInfo &tz_info);

        /**
         * Extract DST transition rules from recent transitions
         *
         * Analyzes recent (last 5 years) transitions to determine DST pattern.
         *
         * @param transitions Vector of timezone transitions
         * @param tz_info Output: timezone info with DST rules filled in
         */
        void extractDSTRules(const std::vector<TimezoneTransition> &transitions,
                             CatalogManager::TimezoneInfo &tz_info);

        /**
         * Calculate week of month from day of month
         *
         * @param day Day of month (1-31)
         * @return Week of month (1-5, or 0 for last week)
         */
        auto calculateWeekOfMonth(int day) -> uint8_t;

        /**
         * Get current Unix timestamp
         *
         * @return Current time as Unix timestamp (seconds since epoch)
         */
        auto getCurrentTimestamp() -> int64_t;
    };

} // namespace scratchbird::core
