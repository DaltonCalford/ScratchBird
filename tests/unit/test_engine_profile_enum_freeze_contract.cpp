#include <gtest/gtest.h>

#include <set>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/engine_profile_contract.h"
#include "scratchbird/ipc/ipc_contract_v1_1.h"
#include "scratchbird/network/socket_types.h"

namespace scratchbird::tests {
namespace {

using Engine = core::CatalogManager::EmulationEngine;
using Protocol = network::ProtocolType;

TEST(EngineProfileEnumFreezeContractTest, EmulationEngineIdsAreFrozen) {
    EXPECT_EQ(static_cast<uint8_t>(Engine::NATIVE), 0u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::FIREBIRD), 1u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::POSTGRESQL), 2u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::MYSQL), 3u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::CASSANDRA), 4u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::MILVUS), 5u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::MONGODB), 6u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::NEO4J), 7u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::REDIS), 8u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::MARIADB), 9u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::INFLUXDB), 10u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::CLICKHOUSE), 11u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::OPENSEARCH), 12u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::DUCKDB), 13u);
    EXPECT_EQ(static_cast<uint8_t>(Engine::UNSPECIFIED), 255u);
    EXPECT_EQ(Engine::FIREBIRDSQL, Engine::FIREBIRD);
}

TEST(EngineProfileEnumFreezeContractTest, RouteProtocolIdsAreFrozen) {
    EXPECT_EQ(static_cast<uint8_t>(Protocol::NATIVE), 0u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::POSTGRESQL), 1u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::MYSQL), 2u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::FIREBIRD), 3u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::CASSANDRA), 4u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::CLICKHOUSE), 5u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::DUCKDB), 6u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::INFLUXDB), 7u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::MARIADB), 8u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::MILVUS), 9u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::MONGODB), 10u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::NEO4J), 11u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::OPENSEARCH), 12u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::REDIS), 13u);
    EXPECT_EQ(static_cast<uint8_t>(Protocol::AUTO_DETECT), 255u);
    EXPECT_EQ(Protocol::FIREBIRDSQL, Protocol::FIREBIRD);
}

TEST(EngineProfileEnumFreezeContractTest, CanonicalVnextSetMapsToDeterministicProtocols) {
    const auto& engines = core::profile_contract::canonicalEmulationEngineSet();
    ASSERT_EQ(engines.size(), 13u);

    std::set<uint8_t> seen_engine_ids;
    std::set<uint8_t> seen_protocol_ids;

    for (Engine engine : engines) {
        EXPECT_TRUE(core::profile_contract::isCanonicalEmulationEngine(engine));

        const auto engine_id = static_cast<uint8_t>(engine);
        seen_engine_ids.insert(engine_id);

        Protocol protocol = core::profile_contract::emulationEngineToRouteProtocol(engine);
        ASSERT_NE(protocol, Protocol::AUTO_DETECT);
        seen_protocol_ids.insert(static_cast<uint8_t>(protocol));

        const char* protocol_name = network::protocolTypeToString(protocol);
        ASSERT_NE(protocol_name, nullptr);
        EXPECT_STRNE(protocol_name, "unknown");
    }

    EXPECT_EQ(seen_engine_ids.size(), engines.size());
    EXPECT_EQ(seen_protocol_ids.size(), engines.size());
}

TEST(EngineProfileEnumFreezeContractTest, IpcCapabilityBaselineMaskIsStable) {
    constexpr uint32_t expected_mask =
        ipc::IPC_FEATURE_PREPARED_STATEMENTS |
        ipc::IPC_FEATURE_COPY_STREAMING |
        ipc::IPC_FEATURE_NOTIFICATIONS |
        ipc::IPC_FEATURE_CANCEL |
        ipc::IPC_FEATURE_BINARY_RESULTS |
        ipc::IPC_FEATURE_COMPRESSION |
        ipc::IPC_FEATURE_ENCRYPTION |
        ipc::IPC_FEATURE_BATCH_EXECUTION;

    EXPECT_EQ(core::profile_contract::kIpcFeatureContractMaskV11, expected_mask);
    EXPECT_EQ(core::profile_contract::kIpcFeatureContractMaskV11, 0xFFu);
}

} // namespace
} // namespace scratchbird::tests

