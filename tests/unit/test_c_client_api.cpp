#include <cstdlib>

#include <gtest/gtest.h>

#include "scratchbird/client/scratchbird_client.h"

TEST(CClientApi, NullInputs) {
    sb_error err{};
    auto* conn = sb_connect(nullptr, &err);
    EXPECT_EQ(conn, nullptr);
    EXPECT_NE(err.code, SB_OK);

    auto* result = sb_execute(nullptr, "SELECT 1", &err);
    EXPECT_EQ(result, nullptr);
    EXPECT_NE(err.code, SB_OK);

    sb_row row{};
    int fetch_status = sb_fetch(nullptr, &row, &err);
    EXPECT_NE(fetch_status, SB_OK);
}

TEST(CClientApi, IntegrationSelect) {
    const char* dsn = std::getenv("SCRATCHBIRD_C_API_URL");
    if (!dsn || !*dsn) {
        GTEST_SKIP() << "SCRATCHBIRD_C_API_URL not set";
    }
    sb_error err{};
    auto* conn = sb_connect(dsn, &err);
    ASSERT_NE(conn, nullptr) << err.message;

    auto* result = sb_execute(conn, "SELECT 1", &err);
    ASSERT_NE(result, nullptr) << err.message;

    sb_row row{};
    ASSERT_EQ(sb_fetch(result, &row, &err), SB_OK);
    int64_t value = 0;
    EXPECT_EQ(sb_get_int64(&row, 0, &value), SB_OK);
    EXPECT_EQ(value, 1);

    sb_result_free(result);
    sb_disconnect(conn);
}
