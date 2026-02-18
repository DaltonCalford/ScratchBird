#include <gtest/gtest.h>

#include <filesystem>
#include <set>
#include <string>

#include "scratchbird/core/engine_profile_contract.h"
#include "scratchbird/protocol/adapters/native_adapter.h"
#include "scratchbird/protocol/sbwp_protocol.h"

namespace scratchbird::tests {
namespace {

std::filesystem::path dbPath(const std::string& name) {
    return std::filesystem::path("build") / "database" / name;
}

void cleanupDb(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(dbPath(name), ec);
    std::filesystem::create_directories(dbPath(name).parent_path(), ec);
}

class NativeAdapterHarness : public protocol::NativeAdapter {
public:
    using protocol::NativeAdapter::NativeAdapter;

    uint64_t advertisedFeatureMask() const {
        return contractServerFeatureMask();
    }
};

TEST(EngineApiCompatibilityContractTest, CanonicalEngineProfilesStayInCrossLayerLockstep) {
    const auto& engines = core::profile_contract::canonicalEmulationEngineSet();
    const auto& registry = protocol::sbwp::canonicalProfileFeatureRegistry();
    ASSERT_EQ(engines.size(), registry.size());

    std::set<std::string> registry_profiles;
    for (const auto& entry : registry) {
        registry_profiles.emplace(entry.first);
    }

    for (const auto engine : engines) {
        const char* profile_id = core::profile_contract::emulationEngineProfileId(engine);
        ASSERT_NE(profile_id, nullptr);
        EXPECT_TRUE(registry_profiles.find(profile_id) != registry_profiles.end())
            << "missing profile id in SBWP capability registry: " << profile_id;

        EXPECT_TRUE(protocol::sbwp::hasProfileFeature(protocol::sbwp::kFeatureProfileMask, profile_id));

        const auto route = core::profile_contract::emulationEngineToRouteProtocol(engine);
        EXPECT_NE(route, network::ProtocolType::AUTO_DETECT);
        EXPECT_STRNE(network::protocolTypeToString(route), "unknown");
        EXPECT_NE(network::getDefaultPort(route), 0u);
    }
}

TEST(EngineApiCompatibilityContractTest, NativeAdapterAdvertisesCanonicalProfileCapabilityMask) {
    cleanupDb("test_engine_api_contract_native_mask.sbdb");

    protocol::ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_engine_api_contract_native_mask.sbdb").string();

    NativeAdapterHarness adapter(cfg);
    const uint64_t mask = adapter.advertisedFeatureMask();
    const uint64_t advertised_profile_mask = mask & protocol::sbwp::kFeatureProfileMask;

    EXPECT_EQ(advertised_profile_mask, protocol::sbwp::canonicalProfileFeatureMask());

    const auto enabled_profiles = protocol::sbwp::enabledProfilesFromFeatureMask(mask);
    EXPECT_EQ(enabled_profiles.size(), 13u);
    EXPECT_TRUE(protocol::sbwp::hasProfileFeature(mask, "postgresql"));
    EXPECT_TRUE(protocol::sbwp::hasProfileFeature(mask, "firebird"));
    EXPECT_TRUE(protocol::sbwp::hasProfileFeature(mask, "opensearch"));
}

TEST(EngineApiCompatibilityContractTest, ContractVersionAndRouteProtocolSurfaceAreStable) {
    EXPECT_EQ(core::profile_contract::kEngineProfileContractVersion, 0x0001);
    EXPECT_EQ(ipc::IPC_CURRENT_VERSION, ipc::IPC_VERSION_1_1);

    const auto& route_set = core::profile_contract::canonicalRouteProtocolSet();
    ASSERT_EQ(route_set.size(), 14u);
    EXPECT_EQ(route_set.front(), network::ProtocolType::NATIVE);
    EXPECT_EQ(route_set.back(), network::ProtocolType::REDIS);
}

} // namespace
} // namespace scratchbird::tests

