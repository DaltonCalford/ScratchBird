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

/**
 * @file firebird_datetime.h
 * @brief Centralized Firebird-compatible date/time utilities
 *
 * ScratchBird uses Firebird's date/time storage formats throughout:
 * - DATE: Modified Julian Date (MJD) - int32, days since November 17, 1858
 * - TIME: Deci-milliseconds - int32, 100µs units since midnight (0-863999999)
 * - TIMESTAMP: MJD date (int32) + deci-ms time (int32) = 8 bytes total
 * - TIME WITH TIME ZONE: UTC time (int32) + timezone_id (uint16)
 * - TIMESTAMP WITH TIME ZONE: UTC timestamp (8 bytes) + timezone_id (uint16)
 *
 * All timezone-aware types store UTC internally and convert to local time for display.
 */

#include <cstdint>
#include <ctime>
#include <chrono>
#include <string>
#include <cstdio>

namespace scratchbird::core
{

/**
 * Firebird date/time constants and utilities
 */
namespace FirebirdDateTime
{
    // Modified Julian Date epoch: November 17, 1858
    // Unix epoch (January 1, 1970) = MJD 40587
    constexpr int32_t UNIX_EPOCH_MJD = 40587;

    // Time units: 10,000 deci-milliseconds per second
    constexpr int32_t DECI_MS_PER_SECOND = 10000;
    constexpr int32_t DECI_MS_PER_MINUTE = DECI_MS_PER_SECOND * 60;
    constexpr int32_t DECI_MS_PER_HOUR = DECI_MS_PER_MINUTE * 60;
    constexpr int32_t DECI_MS_PER_DAY = DECI_MS_PER_HOUR * 24;  // 864,000,000

    // Seconds per day
    constexpr int32_t SECONDS_PER_DAY = 86400;

    /**
     * Convert a calendar date to Modified Julian Date
     * @param year Year (e.g., 2024)
     * @param month Month (1-12)
     * @param day Day of month (1-31)
     * @return MJD value
     */
    inline int32_t dateToMJD(int year, int month, int day)
    {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = 0;
        std::time_t time = timegm(&tm);  // Use UTC

        // Floor division for negative timestamps (dates before 1970)
        int32_t unix_days = (time >= 0)
            ? static_cast<int32_t>(time / SECONDS_PER_DAY)
            : static_cast<int32_t>((time - SECONDS_PER_DAY + 1) / SECONDS_PER_DAY);

        return unix_days + UNIX_EPOCH_MJD;
    }

    /**
     * Convert Modified Julian Date to calendar date
     * @param mjd MJD value
     * @param year Output year
     * @param month Output month (1-12)
     * @param day Output day of month (1-31)
     */
    inline void mjdToDate(int32_t mjd, int& year, int& month, int& day)
    {
        int32_t unix_days = mjd - UNIX_EPOCH_MJD;
        std::time_t time = static_cast<std::time_t>(unix_days) * SECONDS_PER_DAY;
        std::tm* tm = std::gmtime(&time);
        year = tm->tm_year + 1900;
        month = tm->tm_mon + 1;
        day = tm->tm_mday;
    }

    /**
     * Convert time components to deci-milliseconds
     * @param hour Hour (0-23)
     * @param minute Minute (0-59)
     * @param second Second (0-59)
     * @param fraction Sub-second fraction (0-9999, in 100µs units)
     * @return Deci-milliseconds since midnight
     */
    inline int32_t timeToDeciMs(int hour, int minute, int second, int fraction = 0)
    {
        return hour * DECI_MS_PER_HOUR + minute * DECI_MS_PER_MINUTE +
               second * DECI_MS_PER_SECOND + fraction;
    }

    /**
     * Convert deci-milliseconds to time components
     * @param deci_ms Deci-milliseconds since midnight
     * @param hour Output hour (0-23)
     * @param minute Output minute (0-59)
     * @param second Output second (0-59)
     * @param fraction Output sub-second fraction (0-9999)
     */
    inline void deciMsToTime(int32_t deci_ms, int& hour, int& minute, int& second, int& fraction)
    {
        fraction = deci_ms % DECI_MS_PER_SECOND;
        int32_t total_seconds = deci_ms / DECI_MS_PER_SECOND;
        hour = total_seconds / 3600;
        minute = (total_seconds % 3600) / 60;
        second = total_seconds % 60;
    }

    /**
     * Convert Unix timestamp (seconds since 1970) to Firebird MJD + deci-ms
     * @param unix_seconds Unix timestamp in seconds
     * @param mjd_date Output MJD date
     * @param deci_ms_time Output deci-milliseconds time
     */
    inline void unixToFirebird(int64_t unix_seconds, int32_t& mjd_date, int32_t& deci_ms_time)
    {
        int32_t unix_days = static_cast<int32_t>(unix_seconds / SECONDS_PER_DAY);
        int32_t day_seconds = static_cast<int32_t>(unix_seconds % SECONDS_PER_DAY);
        if (day_seconds < 0) {
            unix_days--;
            day_seconds += SECONDS_PER_DAY;
        }
        mjd_date = unix_days + UNIX_EPOCH_MJD;
        deci_ms_time = day_seconds * DECI_MS_PER_SECOND;
    }

    /**
     * Convert Firebird MJD + deci-ms to Unix timestamp (seconds since 1970)
     * @param mjd_date MJD date
     * @param deci_ms_time Deci-milliseconds time
     * @return Unix timestamp in seconds
     */
    inline int64_t firebirdToUnix(int32_t mjd_date, int32_t deci_ms_time)
    {
        int32_t unix_days = mjd_date - UNIX_EPOCH_MJD;
        int32_t seconds_of_day = deci_ms_time / DECI_MS_PER_SECOND;
        return static_cast<int64_t>(unix_days) * SECONDS_PER_DAY + seconds_of_day;
    }

    /**
     * Get current UTC time as Firebird MJD + deci-ms
     * @param mjd_date Output MJD date
     * @param deci_ms_time Output deci-milliseconds time
     */
    inline void currentUTC(int32_t& mjd_date, int32_t& deci_ms_time)
    {
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();

        int32_t unix_days = static_cast<int32_t>(seconds / SECONDS_PER_DAY);
        int32_t day_seconds = static_cast<int32_t>(seconds % SECONDS_PER_DAY);
        int32_t sub_second_deci_ms = static_cast<int32_t>((micros % 1000000) / 100);

        mjd_date = unix_days + UNIX_EPOCH_MJD;
        deci_ms_time = day_seconds * DECI_MS_PER_SECOND + sub_second_deci_ms;
    }

    /**
     * Format DATE as string (YYYY-MM-DD)
     * @param mjd MJD value
     * @return Formatted date string
     */
    inline std::string formatDate(int32_t mjd)
    {
        int year, month, day;
        mjdToDate(mjd, year, month, day);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
        return buf;
    }

    /**
     * Format TIME as string (HH:MM:SS or HH:MM:SS.nnnn)
     * @param deci_ms Deci-milliseconds since midnight
     * @param include_fraction Whether to include fractional seconds
     * @return Formatted time string
     */
    inline std::string formatTime(int32_t deci_ms, bool include_fraction = false)
    {
        int hour, minute, second, fraction;
        deciMsToTime(deci_ms, hour, minute, second, fraction);
        char buf[20];
        if (include_fraction && fraction > 0) {
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%04d", hour, minute, second, fraction);
        } else {
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, minute, second);
        }
        return buf;
    }

    /**
     * Format TIMESTAMP as string (YYYY-MM-DD HH:MM:SS or YYYY-MM-DD HH:MM:SS.nnnn)
     * @param mjd_date MJD date
     * @param deci_ms_time Deci-milliseconds time
     * @param include_fraction Whether to include fractional seconds
     * @return Formatted timestamp string
     */
    inline std::string formatTimestamp(int32_t mjd_date, int32_t deci_ms_time,
                                       bool include_fraction = false)
    {
        return formatDate(mjd_date) + " " + formatTime(deci_ms_time, include_fraction);
    }

    /**
     * Parse DATE string (YYYY-MM-DD) to MJD
     * @param str Date string
     * @return MJD value, or -1 on parse error
     */
    inline int32_t parseDate(const std::string& str)
    {
        int year, month, day;
        if (std::sscanf(str.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
            return dateToMJD(year, month, day);
        }
        return -1;  // Parse error
    }

    /**
     * Parse TIME string (HH:MM:SS or HH:MM:SS.nnnn) to deci-milliseconds
     * @param str Time string
     * @return Deci-milliseconds, or -1 on parse error
     */
    inline int32_t parseTime(const std::string& str)
    {
        int hour, minute, second = 0, fraction = 0;
        int parsed = std::sscanf(str.c_str(), "%d:%d:%d.%d", &hour, &minute, &second, &fraction);
        if (parsed >= 2) {
            return timeToDeciMs(hour, minute, second, fraction);
        }
        return -1;  // Parse error
    }

    /**
     * Apply timezone offset to convert local time to UTC for storage
     * @param deci_ms_local Local time in deci-milliseconds
     * @param offset_minutes Timezone offset in minutes (e.g., -300 for EST)
     * @return UTC time in deci-milliseconds
     */
    inline int32_t localToUTC(int32_t deci_ms_local, int16_t offset_minutes)
    {
        return deci_ms_local - (offset_minutes * DECI_MS_PER_MINUTE);
    }

    /**
     * Apply timezone offset to convert UTC to local time for display
     * @param deci_ms_utc UTC time in deci-milliseconds
     * @param offset_minutes Timezone offset in minutes
     * @return Local time in deci-milliseconds
     */
    inline int32_t utcToLocal(int32_t deci_ms_utc, int16_t offset_minutes)
    {
        return deci_ms_utc + (offset_minutes * DECI_MS_PER_MINUTE);
    }

}  // namespace FirebirdDateTime

}  // namespace scratchbird::core
