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

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

namespace scratchbird::core
{

    namespace
    {

        auto makeEvent(const std::string& type, uint64_t ts) -> StructuredEventRecord
        {
            StructuredEventRecord event{};
            event.event_type = type;
            event.severity = StructuredEventSeverity::INFO;
            event.occurred_at_ms = ts;
            event.epoch.cluster_config_epoch = 10;
            event.epoch.schema_epoch = 11;
            event.epoch.security_epoch = 12;
            event.db_uuid = "db-uuid-1";
            event.node_id = "node-uuid-1";
            event.shard_id = "shard-uuid-1";
            event.message = "event message";
            event.payload_json = "{\"reason\":\"ok\"}";
            return event;
        }

    } // namespace

    TEST(StructuredEventStreamTest, EmitsDeterministicEventIdsAndEpochContext)
    {
        StructuredEventStream stream;
        stream.setMaxInMemory(2);

        std::string event_id_1;
        std::string event_id_2;
        std::string event_id_3;
        ASSERT_EQ(stream.emit(makeEvent("cluster.fencing.reject", 1700001000), &event_id_1), Status::OK);
        ASSERT_EQ(stream.emit(makeEvent("cluster.replication.apply", 1700001001), &event_id_2), Status::OK);
        ASSERT_EQ(stream.emit(makeEvent("cluster.replication.apply", 1700001002), &event_id_3), Status::OK);
        EXPECT_EQ(event_id_1, "evt-1");
        EXPECT_EQ(event_id_2, "evt-2");
        EXPECT_EQ(event_id_3, "evt-3");

        std::vector<std::string> lines;
        ASSERT_EQ(stream.exportJsonLines(lines), Status::OK);
        ASSERT_EQ(lines.size(), 2u);

        const nlohmann::json first = nlohmann::json::parse(lines[0]);
        const nlohmann::json second = nlohmann::json::parse(lines[1]);
        EXPECT_EQ(first.at("event_id"), "evt-2");
        EXPECT_EQ(second.at("event_id"), "evt-3");
        EXPECT_EQ(first.at("cluster_config_epoch"), 10);
        EXPECT_EQ(first.at("schema_epoch"), 11);
        EXPECT_EQ(first.at("security_epoch"), 12);
        EXPECT_TRUE(first.contains("payload"));

        std::vector<std::string> schema;
        ASSERT_EQ(stream.schemaRegistry(schema), Status::OK);
        ASSERT_EQ(schema.size(), 2u);
        EXPECT_EQ(schema[0], "cluster.fencing.reject");
        EXPECT_EQ(schema[1], "cluster.replication.apply");
    }

    TEST(StructuredEventStreamTest, RejectsMissingEpochOrInvalidPayload)
    {
        StructuredEventRecord invalid = makeEvent("cluster.fencing.reject", 1700002000);
        invalid.epoch.schema_epoch = 0;
        EXPECT_EQ(StructuredEventStream::validate(invalid), Status::INVALID_ARGUMENT);

        invalid = makeEvent("cluster.fencing.reject", 1700002001);
        invalid.payload_json = "not-json";
        EXPECT_EQ(StructuredEventStream::validate(invalid), Status::INVALID_ARGUMENT);

        StructuredEventStream stream;
        invalid = makeEvent("cluster.fencing.reject", 1700002002);
        invalid.message.clear();
        EXPECT_EQ(stream.emit(invalid), Status::INVALID_ARGUMENT);
    }

} // namespace scratchbird::core
