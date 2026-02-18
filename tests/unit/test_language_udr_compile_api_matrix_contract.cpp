/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/udr/language_udr_runtime.h"
#include "scratchbird/udr/language_udr_test_harness.h"

namespace
{
    using scratchbird::core::ErrorContext;
    using scratchbird::core::Status;
    using scratchbird::core::generateUuidV7;
    using scratchbird::udr::LanguageUdrCompileRequest;
    using scratchbird::udr::LanguageUdrCompileResponse;
    using scratchbird::udr::LanguageUdrModuleStatus;
    using scratchbird::udr::LanguageUdrRegistration;
    using scratchbird::udr::LanguageUdrRegistry;
    using scratchbird::udr::LanguageUdrResultShape;
    using scratchbird::udr::LanguageUdrRuntimeBoundary;
    using scratchbird::udr::LanguageUdrSignatureStatus;
    using scratchbird::udr::LanguageUdrTestHarness;

    struct EngineContractCase
    {
        const char *profile_id;
        const char *profile_version;
        const char *translation_mode;
        const char *payload_format;
    };

    static const EngineContractCase kEngineContractCases[] = {
        {"Cassandra", "1.0", "CQL_REWRITE_TO_NATIVE", "CQL_TEXT"},
        {"ClickHouse", "1.0", "SQL_REWRITE_TO_NATIVE", "SQL_TEXT"},
        {"DuckDB", "1.0", "SQL_REWRITE_TO_NATIVE", "SQL_TEXT"},
        {"FirebirdSQL", "1.0", "SQL_REWRITE_TO_NATIVE", "SQL_TEXT"},
        {"InfluxDB", "1.0", "TIMESERIES_REWRITE_TO_NATIVE", "INFLUXQL_TEXT"},
        {"InfluxDB", "1.0", "TIMESERIES_REWRITE_TO_NATIVE", "INFLUX_LINE"},
        {"MariaDB", "1.0", "SQL_REWRITE_TO_NATIVE", "SQL_TEXT"},
        {"Milvus", "1.0", "VECTOR_API_TO_NATIVE", "VECTOR_API_JSON"},
        {"MongoDB", "1.0", "DOCUMENT_PIPELINE_TO_NATIVE", "BSON_COMMAND"},
        {"MySQL", "1.0", "SQL_REWRITE_TO_NATIVE", "SQL_TEXT"},
        {"Neo4j", "1.0", "CYPHER_REWRITE_TO_NATIVE", "CYPHER_TEXT"},
        {"OpenSearch", "1.0", "DSL_REWRITE_TO_NATIVE", "JSON_DSL"},
        {"PostgreSQL", "1.0", "SQL_REWRITE_TO_NATIVE", "SQL_TEXT"},
        {"Redis", "1.0", "RESP_TO_NATIVE_COMMAND", "RESP_ARRAY"},
    };

    auto payloadBytesForFormat(const std::string &payload_format) -> std::vector<uint8_t>
    {
        std::string text;
        if (payload_format == "SQL_TEXT")
        {
            text = "select 1";
        }
        else if (payload_format == "CQL_TEXT")
        {
            text = "select now() from system.local";
        }
        else if (payload_format == "CYPHER_TEXT")
        {
            text = "MATCH (n) RETURN n";
        }
        else if (payload_format == "RESP_ARRAY")
        {
            text = "*1\\r\\n$4\\r\\nPING\\r\\n";
        }
        else if (payload_format == "BSON_COMMAND")
        {
            text = "{\"find\":\"collection\"}";
        }
        else if (payload_format == "JSON_DSL")
        {
            text = "{\"query\":{\"match_all\":{}}}";
        }
        else if (payload_format == "INFLUXQL_TEXT")
        {
            text = "SELECT value FROM cpu";
        }
        else if (payload_format == "INFLUX_LINE")
        {
            text = "cpu,host=a value=1i 10";
        }
        else if (payload_format == "VECTOR_API_JSON")
        {
            text = "{\"op\":\"search\",\"vector\":[0.1,0.2]}";
        }
        else
        {
            text = "payload";
        }
        return std::vector<uint8_t>(text.begin(), text.end());
    }

    auto mismatchedPayloadFormat(const std::string &allowed_format) -> std::string
    {
        if (allowed_format == "RESP_ARRAY")
        {
            return "SQL_TEXT";
        }
        return "RESP_ARRAY";
    }

    auto makeRegistration(const EngineContractCase &c) -> LanguageUdrRegistration
    {
        LanguageUdrRegistration reg{};
        reg.module_id = generateUuidV7();
        reg.module_name = std::string("udr_") + c.profile_id + "_module";
        reg.engine_profile_id = c.profile_id;
        reg.engine_profile_version = c.profile_version;
        reg.translation_mode = c.translation_mode;
        reg.module_semver = "1.0.0";
        reg.artifact_hash = std::string("artifact_") + c.profile_id;
        reg.signature_status = LanguageUdrSignatureStatus::TRUSTED;
        reg.status = LanguageUdrModuleStatus::ACTIVE;
        return reg;
    }

    auto makeRequest(const EngineContractCase &c,
                     const std::string &payload_format) -> LanguageUdrCompileRequest
    {
        LanguageUdrCompileRequest req{};
        req.request_id = generateUuidV7();
        req.profile_id = c.profile_id;
        req.profile_version = c.profile_version;
        req.payload_format = payload_format;
        req.payload = payloadBytesForFormat(payload_format);
        req.session_option_signature = "sess_sig";
        req.principal_id = generateUuidV7();
        req.role_context_signature = "role_sig";
        req.transaction_id = 42;
        req.catalog_epoch = 9;
        req.security_epoch = 11;
        req.native_feature_key = "compile_route";
        req.compile_permission_granted = true;
        return req;
    }
}

TEST(LanguageUdrCompileApiMatrixContractTest, CompileSingleSupportsAllDeclaredEnginePayloadPairs)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    for (const auto &c : kEngineContractCases)
    {
        LanguageUdrRegistration reg = makeRegistration(c);
        ASSERT_EQ(registry.registerModule(reg, {"compile_route"}, &ctx), Status::OK)
            << c.profile_id << " " << c.payload_format << " " << ctx.message;
    }

    LanguageUdrTestHarness harness(registry);
    for (const auto &c : kEngineContractCases)
    {
        LanguageUdrCompileRequest req = makeRequest(c, c.payload_format);
        LanguageUdrCompileResponse response{};
        ASSERT_EQ(harness.compileSingle(req, response, &ctx), Status::OK)
            << c.profile_id << " " << c.payload_format << " " << ctx.message;
        EXPECT_TRUE(response.success) << c.profile_id << " " << c.payload_format;
        EXPECT_EQ(response.result_shape, LanguageUdrResultShape::UDR_RS_COMPILE_OK)
            << c.profile_id << " " << c.payload_format;
    }
}

TEST(LanguageUdrCompileApiMatrixContractTest, PreflightRejectsMismatchedPayloadPerProfileWithUDR1505)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    for (const auto &c : kEngineContractCases)
    {
        LanguageUdrRegistration reg = makeRegistration(c);
        ASSERT_EQ(registry.registerModule(reg, {"compile_route"}, &ctx), Status::OK)
            << c.profile_id << " " << c.payload_format << " " << ctx.message;
    }

    for (const auto &c : kEngineContractCases)
    {
        const std::string wrong_payload = mismatchedPayloadFormat(c.payload_format);
        LanguageUdrCompileRequest req = makeRequest(c, wrong_payload);

        scratchbird::udr::LanguageUdrRegistration selected{};
        ErrorContext local_ctx;
        const Status status = LanguageUdrRuntimeBoundary::preflightCompile(
            registry, req, selected, &local_ctx);
        EXPECT_EQ(status, Status::SYNTAX_ERROR)
            << c.profile_id << " wrong=" << wrong_payload;
        EXPECT_EQ(local_ctx.vnext_code, "UDR_1505")
            << c.profile_id << " wrong=" << wrong_payload;
    }
}

TEST(LanguageUdrCompileApiMatrixContractTest, PreflightRejectsTranslationModeMismatchWithUDR1503)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    EngineContractCase c{"PostgreSQL", "1.0", "CQL_REWRITE_TO_NATIVE", "SQL_TEXT"};
    LanguageUdrRegistration reg = makeRegistration(c);
    ASSERT_EQ(registry.registerModule(reg, {"compile_route"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest(c, c.payload_format);

    scratchbird::udr::LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::NOT_SUPPORTED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1503");
}

TEST(LanguageUdrCompileApiMatrixContractTest, FirebirdAliasMapsToFirebirdSqlContract)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    EngineContractCase c{"FirebirdSQL", "1.0", "SQL_REWRITE_TO_NATIVE", "SQL_TEXT"};
    LanguageUdrRegistration reg = makeRegistration(c);
    ASSERT_EQ(registry.registerModule(reg, {"compile_route"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest(c, c.payload_format);
    req.profile_id = "Firebird";

    scratchbird::udr::LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::OK) << ctx.message;
}
