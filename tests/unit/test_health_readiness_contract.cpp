/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/observability_contract.h"
#include "scratchbird/core/telemetry.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

namespace scratchbird::core
{

    TEST(HealthReadinessContractTest, HealthAndReadinessReflectStateTransitions)
    {
        HealthReadinessContract contract;
        contract.setLivenessState(true, true);
        contract.setReadinessState(false, false, false, false, false, false, false);

        EXPECT_TRUE(contract.isLive());
        EXPECT_FALSE(contract.isReady());

        const std::string health_json = contract.healthzJson(1700000100);
        const std::string ready_json = contract.readyzJson(1700000100);
        const nlohmann::json health = nlohmann::json::parse(health_json);
        const nlohmann::json ready = nlohmann::json::parse(ready_json);

        EXPECT_EQ(health.at("path"), "/healthz");
        EXPECT_EQ(health.at("status"), "OK");
        EXPECT_EQ(ready.at("path"), "/readyz");
        EXPECT_EQ(ready.at("status"), "NOT_READY");
        EXPECT_FALSE(ready.at("ready"));

        contract.setReadinessState(true, true, true, true, true, true, true);
        EXPECT_TRUE(contract.isReady());

        const nlohmann::json ready_after = nlohmann::json::parse(contract.readyzJson(1700000200));
        EXPECT_TRUE(ready_after.at("ready"));
        EXPECT_EQ(ready_after.at("status"), "READY");

        contract.setLivenessState(false, true);
        EXPECT_FALSE(contract.isLive());
        EXPECT_FALSE(contract.isReady());
        const nlohmann::json health_after = nlohmann::json::parse(contract.healthzJson(1700000300));
        EXPECT_EQ(health_after.at("status"), "FAIL");
    }

    TEST(HealthReadinessContractTest, ExportsComponentRowsForSqlHealthView)
    {
        HealthReadinessContract contract;
        contract.setLivenessState(true, true);
        contract.setReadinessState(true, true, true, true, false, true, true);

        std::vector<HealthComponentRow> rows;
        ASSERT_EQ(contract.healthComponentRows(1700000400, rows), Status::OK);
        ASSERT_EQ(rows.size(), 9u);

        bool saw_control_plane_fail = false;
        bool saw_process_ok = false;
        for (const HealthComponentRow& row : rows)
        {
            if (row.component == "control_plane_reachable")
            {
                saw_control_plane_fail = (row.status == HealthComponentStatus::FAIL);
            }
            if (row.component == "process_running")
            {
                saw_process_ok = (row.status == HealthComponentStatus::OK);
            }
        }

        EXPECT_TRUE(saw_control_plane_fail);
        EXPECT_TRUE(saw_process_ok);
    }

    TEST(HealthReadinessContractTest, MetricsEndpointRoutesHealthAndReadinessPaths)
    {
        MetricsEndpoint::setLivenessState(true, true);
        MetricsEndpoint::setReadinessState(true, true, true, true, true, true, true);

        const nlohmann::json health = nlohmann::json::parse(
            MetricsEndpoint::handleRequest("/healthz", "application/json"));
        const nlohmann::json ready = nlohmann::json::parse(
            MetricsEndpoint::handleRequest("/readyz", "application/json"));

        EXPECT_EQ(health.at("path"), "/healthz");
        EXPECT_EQ(health.at("status"), "OK");
        EXPECT_EQ(ready.at("path"), "/readyz");
        EXPECT_EQ(ready.at("status"), "READY");

        const std::string metrics = MetricsEndpoint::handleRequest(
            "/metrics",
            "application/openmetrics-text");
        EXPECT_NE(metrics.find("# EOF"), std::string::npos);
    }

} // namespace scratchbird::core
