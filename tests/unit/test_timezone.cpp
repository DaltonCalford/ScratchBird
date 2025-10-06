#include <gtest/gtest.h>
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/error_context.h"
#include <vector>
#include <string>

using namespace scratchbird::core;

class TimezoneTest : public ::testing::Test
{
protected:
    TimezoneManager tz_manager;
    ErrorContext error_ctx;
};

// ===== TimezoneOffset Tests =====

TEST_F(TimezoneTest, TimezoneOffset_ToString)
{
    TimezoneOffset utc(0, false);
    EXPECT_EQ(utc.toString(), "+00:00");

    TimezoneOffset est(-5 * 60, false);
    EXPECT_EQ(est.toString(), "-05:00");

    TimezoneOffset pst(-8 * 60, false);
    EXPECT_EQ(pst.toString(), "-08:00");

    TimezoneOffset ist(5 * 60 + 30, false); // India Standard Time
    EXPECT_EQ(ist.toString(), "+05:30");
}

TEST_F(TimezoneTest, TimezoneOffset_FromString_Valid)
{
    auto utc = TimezoneOffset::fromString("+00:00", &error_ctx);
    ASSERT_TRUE(utc.has_value());
    EXPECT_EQ(utc->offset_minutes, 0);

    auto est = TimezoneOffset::fromString("-05:00", &error_ctx);
    ASSERT_TRUE(est.has_value());
    EXPECT_EQ(est->offset_minutes, -5 * 60);

    auto pst = TimezoneOffset::fromString("-0800", &error_ctx); // Without colon
    ASSERT_TRUE(pst.has_value());
    EXPECT_EQ(pst->offset_minutes, -8 * 60);

    auto ist = TimezoneOffset::fromString("+0530", &error_ctx);
    ASSERT_TRUE(ist.has_value());
    EXPECT_EQ(ist->offset_minutes, 5 * 60 + 30);
}

TEST_F(TimezoneTest, TimezoneOffset_FromString_Invalid)
{
    auto invalid1 = TimezoneOffset::fromString("", &error_ctx);
    EXPECT_FALSE(invalid1.has_value());

    auto invalid2 = TimezoneOffset::fromString("ABC", &error_ctx);
    EXPECT_FALSE(invalid2.has_value());

    auto invalid3 = TimezoneOffset::fromString("+25:00", &error_ctx); // Out of range
    EXPECT_FALSE(invalid3.has_value());
}

// ===== TimezoneManager Tests =====

TEST_F(TimezoneTest, TimezoneManager_GetTimezoneInfo)
{
    const auto* utc_info = tz_manager.getTimezoneInfo(TimezoneManager::TZ_UTC);
    ASSERT_NE(utc_info, nullptr);
    EXPECT_EQ(utc_info->name, "UTC");
    EXPECT_EQ(utc_info->abbreviation, "UTC");
    EXPECT_EQ(utc_info->offset.offset_minutes, 0);

    const auto* est_info = tz_manager.getTimezoneInfo(TimezoneManager::TZ_EST);
    ASSERT_NE(est_info, nullptr);
    EXPECT_EQ(est_info->name, "America/New_York");
    EXPECT_EQ(est_info->abbreviation, "EST");
    EXPECT_EQ(est_info->offset.offset_minutes, -5 * 60);
}

TEST_F(TimezoneTest, TimezoneManager_GetTimezoneByName)
{
    EXPECT_EQ(tz_manager.getTimezoneByName("UTC"), TimezoneManager::TZ_UTC);
    EXPECT_EQ(tz_manager.getTimezoneByName("GMT"), TimezoneManager::TZ_UTC);
    EXPECT_EQ(tz_manager.getTimezoneByName("America/New_York"), TimezoneManager::TZ_EST);
    EXPECT_EQ(tz_manager.getTimezoneByName("EST"), TimezoneManager::TZ_EST);
    EXPECT_EQ(tz_manager.getTimezoneByName("PST"), TimezoneManager::TZ_PST);
}

TEST_F(TimezoneTest, TimezoneManager_GetTimezoneByAbbreviation)
{
    EXPECT_EQ(tz_manager.getTimezoneByAbbreviation("UTC"), TimezoneManager::TZ_UTC);
    EXPECT_EQ(tz_manager.getTimezoneByAbbreviation("EST"), TimezoneManager::TZ_EST);
    EXPECT_EQ(tz_manager.getTimezoneByAbbreviation("PST"), TimezoneManager::TZ_PST);
}

// ===== Timezone Conversion Tests =====

TEST_F(TimezoneTest, ToGMT_FromEST)
{
    // 10:00 AM EST -> 3:00 PM GMT (15:00)
    // In microseconds: 10 hours = 10 * 3600 * 1000000
    int64_t est_time = 10LL * 3600 * 1000000; // 10:00 AM as microseconds since midnight
    auto gmt_time = tz_manager.toGMT(est_time, TimezoneManager::TZ_EST, &error_ctx);

    ASSERT_TRUE(gmt_time.has_value());
    int64_t expected_gmt = 15LL * 3600 * 1000000; // 15:00 (3:00 PM)
    EXPECT_EQ(*gmt_time, expected_gmt);
}

TEST_F(TimezoneTest, FromGMT_ToEST)
{
    // 15:00 GMT (3:00 PM) -> 10:00 AM EST
    int64_t gmt_time = 15LL * 3600 * 1000000;
    auto est_time = tz_manager.fromGMT(gmt_time, TimezoneManager::TZ_EST, &error_ctx);

    ASSERT_TRUE(est_time.has_value());
    int64_t expected_est = 10LL * 3600 * 1000000; // 10:00 AM
    EXPECT_EQ(*est_time, expected_est);
}

TEST_F(TimezoneTest, ToGMT_FromPST)
{
    // 10:00 AM PST -> 6:00 PM GMT (18:00)
    int64_t pst_time = 10LL * 3600 * 1000000;
    auto gmt_time = tz_manager.toGMT(pst_time, TimezoneManager::TZ_PST, &error_ctx);

    ASSERT_TRUE(gmt_time.has_value());
    int64_t expected_gmt = 18LL * 3600 * 1000000; // 18:00 (6:00 PM)
    EXPECT_EQ(*gmt_time, expected_gmt);
}

// ===== Timestamp Parsing Tests =====

TEST_F(TimezoneTest, ParseTimestamp_WithUTCOffset)
{
    // Parse: "2025-10-04 15:30:00+00:00"
    // Should return timestamp in GMT
    auto ts = tz_manager.parseTimestamp("2025-10-04 15:30:00+00:00",
                                        TimezoneManager::TZ_UTC, &error_ctx);
    ASSERT_TRUE(ts.has_value());
    EXPECT_GT(*ts, 0);
}

TEST_F(TimezoneTest, ParseTimestamp_WithESTOffset)
{
    // Parse: "2025-10-04 10:30:00-05:00" (EST)
    // This is 15:30:00 GMT
    auto ts1 = tz_manager.parseTimestamp("2025-10-04 10:30:00-05:00",
                                         TimezoneManager::TZ_UTC, &error_ctx);

    // Parse: "2025-10-04 15:30:00+00:00" (GMT)
    auto ts2 = tz_manager.parseTimestamp("2025-10-04 15:30:00+00:00",
                                         TimezoneManager::TZ_UTC, &error_ctx);

    ASSERT_TRUE(ts1.has_value());
    ASSERT_TRUE(ts2.has_value());

    // Both should be the same GMT time
    EXPECT_EQ(*ts1, *ts2);
}

TEST_F(TimezoneTest, ParseTimestamp_WithNamedTimezone)
{
    // Parse: "2025-10-04 15:30:00 UTC"
    auto ts = tz_manager.parseTimestamp("2025-10-04 15:30:00 UTC",
                                        TimezoneManager::TZ_UTC, &error_ctx);
    ASSERT_TRUE(ts.has_value());
    EXPECT_GT(*ts, 0);
}

TEST_F(TimezoneTest, ParseTimestamp_NoTimezone_UsesDefault)
{
    // Parse: "2025-10-04 15:30:00" (no timezone)
    // Should use default timezone (UTC in this test)
    auto ts = tz_manager.parseTimestamp("2025-10-04 15:30:00",
                                        TimezoneManager::TZ_UTC, &error_ctx);
    ASSERT_TRUE(ts.has_value());
    EXPECT_GT(*ts, 0);
}

TEST_F(TimezoneTest, ParseTimestamp_WithMicroseconds)
{
    // Parse: "2025-10-04 15:30:00.123456+00:00"
    auto ts = tz_manager.parseTimestamp("2025-10-04 15:30:00.123456+00:00",
                                        TimezoneManager::TZ_UTC, &error_ctx);
    ASSERT_TRUE(ts.has_value());

    // Check that microseconds are preserved
    int64_t microseconds = *ts % 1000000;
    EXPECT_EQ(microseconds, 123456);
}

TEST_F(TimezoneTest, ParseTimestamp_Invalid)
{
    auto invalid1 = tz_manager.parseTimestamp("invalid", TimezoneManager::TZ_UTC, &error_ctx);
    EXPECT_FALSE(invalid1.has_value());

    auto invalid2 = tz_manager.parseTimestamp("2025-13-01 00:00:00", TimezoneManager::TZ_UTC, &error_ctx);
    EXPECT_FALSE(invalid2.has_value());
}

// ===== Timestamp Formatting Tests =====

TEST_F(TimezoneTest, FormatTimestamp_UTC)
{
    // Create a known GMT timestamp: 2025-10-04 15:30:00.123456
    struct tm timeinfo = {};
    timeinfo.tm_year = 2025 - 1900;
    timeinfo.tm_mon = 10 - 1; // October
    timeinfo.tm_mday = 4;
    timeinfo.tm_hour = 15;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 0;

    time_t epoch_seconds = timegm(&timeinfo);
    int64_t gmt_microseconds = static_cast<int64_t>(epoch_seconds) * 1000000 + 123456;

    std::string result = tz_manager.formatTimestamp(gmt_microseconds, TimezoneManager::TZ_UTC, true);

    // Should be: "2025-10-04 15:30:00.123456+00:00"
    EXPECT_TRUE(result.find("2025-10-04") != std::string::npos);
    EXPECT_TRUE(result.find("15:30:00") != std::string::npos);
    EXPECT_TRUE(result.find("+00:00") != std::string::npos);
}

TEST_F(TimezoneTest, FormatTimestamp_EST)
{
    // Create GMT timestamp: 2025-01-04 15:30:00 (January - standard time, not DST)
    struct tm timeinfo = {};
    timeinfo.tm_year = 2025 - 1900;
    timeinfo.tm_mon = 1 - 1;  // January (month 0)
    timeinfo.tm_mday = 4;
    timeinfo.tm_hour = 15;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 0;

    time_t epoch_seconds = timegm(&timeinfo);
    int64_t gmt_microseconds = static_cast<int64_t>(epoch_seconds) * 1000000;

    std::string result = tz_manager.formatTimestamp(gmt_microseconds, TimezoneManager::TZ_EST, true);

    // Should convert to EST: 10:30:00-05:00 (January is standard time, not DST)
    EXPECT_TRUE(result.find("10:30:00") != std::string::npos);
    EXPECT_TRUE(result.find("-05:00") != std::string::npos);
}

TEST_F(TimezoneTest, FormatTimestamp_WithoutOffset)
{
    struct tm timeinfo = {};
    timeinfo.tm_year = 2025 - 1900;
    timeinfo.tm_mon = 10 - 1;
    timeinfo.tm_mday = 4;
    timeinfo.tm_hour = 15;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 0;

    time_t epoch_seconds = timegm(&timeinfo);
    int64_t gmt_microseconds = static_cast<int64_t>(epoch_seconds) * 1000000;

    std::string result = tz_manager.formatTimestamp(gmt_microseconds, TimezoneManager::TZ_UTC, false);

    // Should NOT include offset
    EXPECT_TRUE(result.find("+00:00") == std::string::npos);
    EXPECT_TRUE(result.find("2025-10-04") != std::string::npos);
    EXPECT_TRUE(result.find("15:30:00") != std::string::npos);
}

// ===== Type Converter Integration Tests =====

TEST_F(TimezoneTest, TypeConverter_StringToTimestamp_WithTimezone)
{
    ErrorContext ctx;

    // Test with explicit UTC offset
    auto ts1 = TypeConverter::stringToTimestamp("2025-10-04 15:30:00+00:00", &ctx);
    ASSERT_TRUE(ts1.has_value());

    // Test with EST offset
    auto ts2 = TypeConverter::stringToTimestamp("2025-10-04 10:30:00-05:00", &ctx);
    ASSERT_TRUE(ts2.has_value());

    // Both should be the same GMT time
    EXPECT_EQ(*ts1, *ts2);
}

TEST_F(TimezoneTest, TypeConverter_TimestampToString_ShowsUTC)
{
    // Create a known timestamp
    struct tm timeinfo = {};
    timeinfo.tm_year = 2025 - 1900;
    timeinfo.tm_mon = 10 - 1;
    timeinfo.tm_mday = 4;
    timeinfo.tm_hour = 15;
    timeinfo.tm_min = 30;
    timeinfo.tm_sec = 0;

    time_t epoch_seconds = timegm(&timeinfo);
    int64_t microseconds = static_cast<int64_t>(epoch_seconds) * 1000000;

    std::string result = TypeConverter::timestampToString(microseconds);

    // Should include UTC offset
    EXPECT_TRUE(result.find("+00:00") != std::string::npos);
    EXPECT_TRUE(result.find("2025-10-04") != std::string::npos);
}

// ===== Round-Trip Tests =====

TEST_F(TimezoneTest, RoundTrip_ParseAndFormat)
{
    std::string original = "2025-10-04 15:30:00.123456+00:00";

    // Parse to GMT
    auto ts = tz_manager.parseTimestamp(original, TimezoneManager::TZ_UTC, &error_ctx);
    ASSERT_TRUE(ts.has_value());

    // Format back
    std::string formatted = tz_manager.formatTimestamp(*ts, TimezoneManager::TZ_UTC, true);

    // Should be identical (or very close)
    EXPECT_TRUE(formatted.find("2025-10-04") != std::string::npos);
    EXPECT_TRUE(formatted.find("15:30:00") != std::string::npos);
    EXPECT_TRUE(formatted.find("123456") != std::string::npos);
    EXPECT_TRUE(formatted.find("+00:00") != std::string::npos);
}

TEST_F(TimezoneTest, RoundTrip_CrossTimezone)
{
    // Parse in EST (using January date for standard time, not DST)
    auto ts = tz_manager.parseTimestamp("2025-01-04 10:30:00-05:00",
                                        TimezoneManager::TZ_UTC, &error_ctx);
    ASSERT_TRUE(ts.has_value());

    // Format in UTC - should show 15:30:00
    std::string utc_result = tz_manager.formatTimestamp(*ts, TimezoneManager::TZ_UTC, true);
    EXPECT_TRUE(utc_result.find("15:30:00") != std::string::npos);

    // Format in PST - should show 07:30:00 (15:00 - 8 hours, January is standard time)
    std::string pst_result = tz_manager.formatTimestamp(*ts, TimezoneManager::TZ_PST, true);
    EXPECT_TRUE(pst_result.find("07:30:00") != std::string::npos);
}
