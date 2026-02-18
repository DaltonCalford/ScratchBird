/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

#include <array>
#include <cstdint>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/ipc/ipc_contract_v1_1.h"
#include "scratchbird/network/socket_types.h"

namespace scratchbird::core::profile_contract {

using EmulationEngine = CatalogManager::EmulationEngine;
using RouteProtocol = network::ProtocolType;

constexpr uint16_t kEngineProfileContractVersion = 0x0001;

// Canonical vNext emulation set (13 engines, no native row).
constexpr std::array<EmulationEngine, 13> kCanonicalEmulationEngineSet{
    EmulationEngine::CASSANDRA,
    EmulationEngine::CLICKHOUSE,
    EmulationEngine::DUCKDB,
    EmulationEngine::FIREBIRD,
    EmulationEngine::INFLUXDB,
    EmulationEngine::MARIADB,
    EmulationEngine::MILVUS,
    EmulationEngine::MONGODB,
    EmulationEngine::MYSQL,
    EmulationEngine::NEO4J,
    EmulationEngine::OPENSEARCH,
    EmulationEngine::POSTGRESQL,
    EmulationEngine::REDIS,
};

// Route protocols include native plus all emulation families.
constexpr std::array<RouteProtocol, 14> kRouteProtocolSet{
    RouteProtocol::NATIVE,
    RouteProtocol::POSTGRESQL,
    RouteProtocol::MYSQL,
    RouteProtocol::FIREBIRD,
    RouteProtocol::CASSANDRA,
    RouteProtocol::CLICKHOUSE,
    RouteProtocol::DUCKDB,
    RouteProtocol::INFLUXDB,
    RouteProtocol::MARIADB,
    RouteProtocol::MILVUS,
    RouteProtocol::MONGODB,
    RouteProtocol::NEO4J,
    RouteProtocol::OPENSEARCH,
    RouteProtocol::REDIS,
};

constexpr auto canonicalEmulationEngineSet() -> const std::array<EmulationEngine, 13>& {
    return kCanonicalEmulationEngineSet;
}

constexpr auto canonicalRouteProtocolSet() -> const std::array<RouteProtocol, 14>& {
    return kRouteProtocolSet;
}

constexpr bool isCanonicalEmulationEngine(EmulationEngine engine) {
    for (EmulationEngine value : kCanonicalEmulationEngineSet) {
        if (value == engine) {
            return true;
        }
    }
    return false;
}

constexpr auto emulationEngineToRouteProtocol(EmulationEngine engine) -> RouteProtocol {
    switch (engine) {
        case EmulationEngine::NATIVE:
            return RouteProtocol::NATIVE;
        case EmulationEngine::POSTGRESQL:
            return RouteProtocol::POSTGRESQL;
        case EmulationEngine::MYSQL:
            return RouteProtocol::MYSQL;
        case EmulationEngine::FIREBIRD:
            return RouteProtocol::FIREBIRD;
        case EmulationEngine::CASSANDRA:
            return RouteProtocol::CASSANDRA;
        case EmulationEngine::CLICKHOUSE:
            return RouteProtocol::CLICKHOUSE;
        case EmulationEngine::DUCKDB:
            return RouteProtocol::DUCKDB;
        case EmulationEngine::INFLUXDB:
            return RouteProtocol::INFLUXDB;
        case EmulationEngine::MARIADB:
            return RouteProtocol::MARIADB;
        case EmulationEngine::MILVUS:
            return RouteProtocol::MILVUS;
        case EmulationEngine::MONGODB:
            return RouteProtocol::MONGODB;
        case EmulationEngine::NEO4J:
            return RouteProtocol::NEO4J;
        case EmulationEngine::OPENSEARCH:
            return RouteProtocol::OPENSEARCH;
        case EmulationEngine::REDIS:
            return RouteProtocol::REDIS;
        case EmulationEngine::UNSPECIFIED:
            return RouteProtocol::AUTO_DETECT;
    }
    return RouteProtocol::AUTO_DETECT;
}

constexpr auto emulationEngineProfileId(EmulationEngine engine) -> const char* {
    switch (engine) {
        case EmulationEngine::POSTGRESQL:
            return "postgresql";
        case EmulationEngine::MYSQL:
            return "mysql";
        case EmulationEngine::FIREBIRD:
            return "firebirdsql";
        case EmulationEngine::CASSANDRA:
            return "cassandra";
        case EmulationEngine::CLICKHOUSE:
            return "clickhouse";
        case EmulationEngine::DUCKDB:
            return "duckdb";
        case EmulationEngine::INFLUXDB:
            return "influxdb";
        case EmulationEngine::MARIADB:
            return "mariadb";
        case EmulationEngine::MILVUS:
            return "milvus";
        case EmulationEngine::MONGODB:
            return "mongodb";
        case EmulationEngine::NEO4J:
            return "neo4j";
        case EmulationEngine::OPENSEARCH:
            return "opensearch";
        case EmulationEngine::REDIS:
            return "redis";
        case EmulationEngine::NATIVE:
            return "native";
        case EmulationEngine::UNSPECIFIED:
            return "unspecified";
    }
    return "unspecified";
}

constexpr uint32_t kIpcFeatureContractMaskV11 =
    ipc::IPC_FEATURE_PREPARED_STATEMENTS |
    ipc::IPC_FEATURE_COPY_STREAMING |
    ipc::IPC_FEATURE_NOTIFICATIONS |
    ipc::IPC_FEATURE_CANCEL |
    ipc::IPC_FEATURE_BINARY_RESULTS |
    ipc::IPC_FEATURE_COMPRESSION |
    ipc::IPC_FEATURE_ENCRYPTION |
    ipc::IPC_FEATURE_BATCH_EXECUTION;

// Frozen enum ID contract (EF-040).
static_assert(static_cast<uint8_t>(EmulationEngine::NATIVE) == 0, "EmulationEngine::NATIVE id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::FIREBIRD) == 1, "EmulationEngine::FIREBIRD id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::POSTGRESQL) == 2, "EmulationEngine::POSTGRESQL id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::MYSQL) == 3, "EmulationEngine::MYSQL id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::CASSANDRA) == 4, "EmulationEngine::CASSANDRA id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::MILVUS) == 5, "EmulationEngine::MILVUS id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::MONGODB) == 6, "EmulationEngine::MONGODB id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::NEO4J) == 7, "EmulationEngine::NEO4J id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::REDIS) == 8, "EmulationEngine::REDIS id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::MARIADB) == 9, "EmulationEngine::MARIADB id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::INFLUXDB) == 10, "EmulationEngine::INFLUXDB id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::CLICKHOUSE) == 11, "EmulationEngine::CLICKHOUSE id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::OPENSEARCH) == 12, "EmulationEngine::OPENSEARCH id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::DUCKDB) == 13, "EmulationEngine::DUCKDB id changed");
static_assert(static_cast<uint8_t>(EmulationEngine::UNSPECIFIED) == 255, "EmulationEngine::UNSPECIFIED id changed");

static_assert(static_cast<uint8_t>(RouteProtocol::NATIVE) == 0, "RouteProtocol::NATIVE id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::POSTGRESQL) == 1, "RouteProtocol::POSTGRESQL id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::MYSQL) == 2, "RouteProtocol::MYSQL id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::FIREBIRD) == 3, "RouteProtocol::FIREBIRD id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::CASSANDRA) == 4, "RouteProtocol::CASSANDRA id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::CLICKHOUSE) == 5, "RouteProtocol::CLICKHOUSE id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::DUCKDB) == 6, "RouteProtocol::DUCKDB id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::INFLUXDB) == 7, "RouteProtocol::INFLUXDB id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::MARIADB) == 8, "RouteProtocol::MARIADB id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::MILVUS) == 9, "RouteProtocol::MILVUS id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::MONGODB) == 10, "RouteProtocol::MONGODB id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::NEO4J) == 11, "RouteProtocol::NEO4J id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::OPENSEARCH) == 12, "RouteProtocol::OPENSEARCH id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::REDIS) == 13, "RouteProtocol::REDIS id changed");
static_assert(static_cast<uint8_t>(RouteProtocol::AUTO_DETECT) == 255, "RouteProtocol::AUTO_DETECT id changed");

static_assert(kIpcFeatureContractMaskV11 == 0xFFu, "IPC feature baseline mask changed");

} // namespace scratchbird::core::profile_contract
