#include <gtest/gtest.h>

#include "scratchbird/core/secure_diagnostics.h"
#include "scratchbird/core/structured_logger.h"

using namespace scratchbird::core;

TEST(SecureDiagnosticsTest, FieldNameRedactionMasksSecretsInStructuredLogs)
{
    StructuredLogEntry entry(LogLevel::INFO, LogCategory::STORAGE);
    entry.message("login failed password=hunter2 endpoint=https://db.internal:8443/auth");
    entry.field("api_token", "super-secret-token");
    entry.error("AUTH_001", "Authorization: Bearer abc.def.ghi");

    const std::string json = entry.toJson();
    EXPECT_EQ(json.find("hunter2"), std::string::npos);
    EXPECT_EQ(json.find("db.internal"), std::string::npos);
    EXPECT_EQ(json.find("super-secret-token"), std::string::npos);
    EXPECT_EQ(json.find("abc.def.ghi"), std::string::npos);
    EXPECT_NE(json.find("<redacted>"), std::string::npos);
    EXPECT_NE(json.find("<endpoint>"), std::string::npos);
}

TEST(SecureDiagnosticsTest, QueryLogJsonRedactsInlineCredentialMaterial)
{
    QueryLogEntry query;
    query.query_id = "q1";
    query.sql_text = "create user alice identified by 'MyPassword!'";
    query.user = "admin";
    query.database = "scratchbird";
    query.start_time_us = 10;
    query.end_time_us = 20;
    query.rows_returned = 0;
    query.rows_examined = 0;
    query.success = false;
    query.error_code = "AUTH_FAIL";
    query.error_message = "token=abcdef";
    query.plan_summary = "connect https://internal.host:9443";

    const std::string json = query.toJson();
    EXPECT_EQ(json.find("MyPassword!"), std::string::npos);
    EXPECT_EQ(json.find("abcdef"), std::string::npos);
    EXPECT_EQ(json.find("internal.host"), std::string::npos);
    EXPECT_NE(json.find("<redacted>"), std::string::npos);
    EXPECT_NE(json.find("<endpoint>"), std::string::npos);
}

TEST(SecureDiagnosticsTest, GenericTextRedactorMasksUriSecretsAndHeaders)
{
    const std::string redacted = redactSensitiveDiagnosticText(
        "postgres://alice:secretpass@db.internal:5432/main Authorization: Bearer t0k3n");
    EXPECT_EQ(redacted.find("secretpass"), std::string::npos);
    EXPECT_EQ(redacted.find("db.internal"), std::string::npos);
    EXPECT_EQ(redacted.find("t0k3n"), std::string::npos);
    EXPECT_NE(redacted.find("<redacted>"), std::string::npos);
    EXPECT_NE(redacted.find("<endpoint>"), std::string::npos);
}
