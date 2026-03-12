#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include <unistd.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/support_bundle_builder.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/server/ipc_server.h"
#include "test_helpers.h"

using scratchbird::core::CatalogManager;
using scratchbird::core::ConnectionContext;
using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::LockManager;
using scratchbird::core::LockMode;
using scratchbird::core::LockStats;
using scratchbird::core::LockTag;
using scratchbird::core::LockTarget;
using scratchbird::core::Status;
using scratchbird::core::SupportBundleBuilder;
using scratchbird::core::SupportBundleRequest;
using scratchbird::core::SupportBundleResult;
using scratchbird::core::SweepPolicyBinding;
using scratchbird::core::SweepPolicyLane;
using scratchbird::core::SweepScopeKind;
using scratchbird::core::TransactionManager;
using scratchbird::core::generateUuidV7;
using scratchbird::server::IPCClient;
using scratchbird::server::IPCClientConfig;
using scratchbird::server::IPCConnection;
using scratchbird::server::IPCMethod;
using scratchbird::server::IPCServer;
using scratchbird::server::IPCServerConfig;
using scratchbird::testing::TestDatabaseFile;

namespace
{

auto makePageLockTag(uint64_t page_num) -> LockTag
{
    LockTag tag{};
    tag.target_type = LockTarget::LOCK_TARGET_PAGE;
    for (size_t i = 0; i < tag.object_uuid.bytes.size(); ++i)
    {
        tag.object_uuid.bytes[i] = static_cast<uint8_t>(i + 1);
    }
    tag.page_num = page_num;
    return tag;
}

auto isNetworkRestrictedError(const ErrorContext& ctx) -> bool
{
    return ctx.message.find("Operation not permitted") != std::string::npos ||
           ctx.message.find("Permission denied") != std::string::npos;
}

auto reserveTcpPort() -> uint16_t
{
    scratchbird::network::NetworkInitGuard guard;
    auto sock =
        scratchbird::network::Socket::create(scratchbird::network::AddressFamily::IPV4);
    if (!sock)
    {
        return 0;
    }

    ErrorContext ctx;
    scratchbird::network::NetworkAddress addr("127.0.0.1", 0);
    if (sock->bind(addr, &ctx) != Status::OK)
    {
        return 0;
    }

    auto local = sock->getLocalAddress();
    if (!local.has_value())
    {
        return 0;
    }
    return local->port;
}

class SupportBundleChaosTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("reliability_chaos_support", ".sbdb");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
        ASSERT_EQ(conn_->initialize(&ctx), Status::OK) << ctx.message;

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(db_.catalog_manager()->getSchema("PUBLIC", schema_info, &ctx), Status::OK)
            << ctx.message;
        public_schema_id_ = schema_info.schema_id;

        conn_->setCurrentSchemaId(public_schema_id_);
        conn_->set_current_schema("PUBLIC");
        conn_->set_search_path({"PUBLIC"});

        system_user_id_ = db_.catalog_manager()->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id_, ID{}) << ctx.message;
        conn_->setCurrentUser(system_user_id_, true);
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();
        db_file_.reset();
    }

    void createAlertEvidence()
    {
        ErrorContext ctx;

        CatalogManager::AlertRuleCatalogInfo rule{};
        rule.rule_id = generateUuidV7();
        rule.rule_name = "ipc_parser_critical";
        rule.rule_kind = CatalogManager::AlertRuleKind::EVENT;
        rule.severity = CatalogManager::AlertSeverity::CRITICAL;
        rule.has_condition_text = true;
        rule.condition_text = "token=abc password=topsecret";
        rule.throttle_interval_ms = 1000;
        ASSERT_EQ(db_.catalog_manager()->upsertAlertRuleCatalogEntry(rule, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::AlertTargetCatalogInfo target{};
        target.target_id = generateUuidV7();
        target.target_name = "ops_webhook";
        target.target_kind = CatalogManager::AlertTargetKind::WEBHOOK;
        target.endpoint = "https://user:secret@ops.example/internal?token=abc";
        ASSERT_EQ(db_.catalog_manager()->upsertAlertTargetCatalogEntry(target, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::AlertRouteCatalogInfo route{};
        route.route_id = generateUuidV7();
        route.rule_id = rule.rule_id;
        route.target_id = target.target_id;
        route.route_kind = CatalogManager::AlertRouteKind::ESCALATION;
        route.severity_min = CatalogManager::AlertSeverity::INFO;
        route.severity_max = CatalogManager::AlertSeverity::CRITICAL;
        ASSERT_EQ(db_.catalog_manager()->upsertAlertRouteCatalogEntry(route, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::AlertEventCatalogInfo event{};
        event.event_id = generateUuidV7();
        event.rule_id = rule.rule_id;
        event.severity = CatalogManager::AlertSeverity::CRITICAL;
        event.event_state = CatalogManager::AlertEventState::OPEN;
        event.event_time = 1000;
        ASSERT_EQ(db_.catalog_manager()->upsertAlertEventCatalogEntry(event, &ctx), Status::OK)
            << ctx.message;
        event_id_ = event.event_id;
    }

    void createForensicEvidence()
    {
        ErrorContext ctx;

        CatalogManager::PageAuditFindingCatalogInfo finding{};
        finding.finding_id = generateUuidV7();
        finding.scan_mode = "DIAGNOSTIC";
        finding.trigger_source = "SWEEP_BACKGROUND";
        finding.page_id = 42;
        finding.page_type = "heap";
        finding.error_code = "PAGE_CHECKSUM_FAIL";
        finding.severity = "CRITICAL";
        finding.details_json =
            "{\"endpoint\":\"https://ops.example/internal?token=abc\",\"password\":\"topsecret\"}";
        ASSERT_EQ(db_.catalog_manager()->appendPageAuditFindingCatalogEntry(finding, &ctx), Status::OK)
            << ctx.message;

        CatalogManager::ShadowCaptureManifestCatalogInfo manifest{};
        manifest.tx_uuid = event_id_;
        manifest.capture_scope = "TRANSACTION";
        manifest.capture_format = "LOGICAL_TX_SUMMARY";
        manifest.payload_manifest =
            "source_manifest_path=https://user:secret@ops.example/forensic?token=abc\n"
            "password=shadow-secret\n";
        manifest.has_retention_deadline_time = true;
        manifest.retention_deadline_time = 900000;
        ASSERT_EQ(db_.catalog_manager()->appendShadowCaptureManifestCatalogEntry(manifest, &ctx),
                  Status::OK) << ctx.message;
    }

    auto reopenDatabase() -> void
    {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        db_.close();

        ErrorContext ctx;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
        ASSERT_EQ(conn_->initialize(&ctx), Status::OK) << ctx.message;
        conn_->setCurrentSchemaId(public_schema_id_);
        conn_->set_current_schema("PUBLIC");
        conn_->set_search_path({"PUBLIC"});
        conn_->setCurrentUser(system_user_id_, true);
    }

    Database db_;
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<ConnectionContext> conn_;
    ID public_schema_id_{};
    ID system_user_id_{};
    ID event_id_{};
};

TEST_F(SupportBundleChaosTest, RestartKeepsOperationalEvidenceReadableAndRedacted)
{
    createAlertEvidence();
    createForensicEvidence();

    ErrorContext ctx;
    ASSERT_EQ(conn_->commit(&ctx), Status::OK) << ctx.message;

    reopenDatabase();

    SupportBundleBuilder builder(&db_);
    SupportBundleRequest request;
    request.output_path = db_file_->path() + ".chaos_bundle";
    request.readiness.now_time = 61000;

    SupportBundleResult result;
    ASSERT_EQ(builder.generateSupportBundle(request, result, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(result.safety.readiness.state, scratchbird::core::ReadinessHealthState::BLOCKED);
    EXPECT_EQ(result.safety.shadow_capture_manifest_count, 1u);
    EXPECT_EQ(result.safety.page_audit_finding_count, 1u);
    EXPECT_TRUE(result.redaction_enforced);
    EXPECT_GT(result.redacted_field_count, 0u);

    std::ifstream in(request.output_path);
    ASSERT_TRUE(in.is_open());
    const std::string contents((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("event_id=" + event_id_.toString()), std::string::npos);
    EXPECT_NE(contents.find("<redacted>"), std::string::npos);
    EXPECT_EQ(contents.find("topsecret"), std::string::npos);
    EXPECT_EQ(contents.find("shadow-secret"), std::string::npos);
    EXPECT_EQ(contents.find("user:secret"), std::string::npos);

    std::remove(request.output_path.c_str());
}

class DeadlockChaosTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = scratchbird::testing::uniqueTestDbPath("reliability_deadlock_chaos", ".sbrd");
        std::filesystem::remove(test_db_path_);

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path_.string(), 8192, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(test_db_path_.string(), &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_->initializeProcArray(8, &ctx), Status::OK) << ctx.message;

        lock_mgr_ = db_->lock_manager();
        txn_mgr_ = db_->transaction_manager();
        ASSERT_NE(lock_mgr_, nullptr);
        ASSERT_NE(txn_mgr_, nullptr);
    }

    void TearDown() override
    {
        ErrorContext ctx;
        scratchbird::core::ProcArrayManager::shutdown(&ctx);
        db_.reset();
        std::filesystem::remove(test_db_path_);
    }

    std::filesystem::path test_db_path_;
    std::unique_ptr<Database> db_;
    LockManager* lock_mgr_ = nullptr;
    TransactionManager* txn_mgr_ = nullptr;
};

TEST_F(DeadlockChaosTest, DeadlockFaultInjectionResolvesWithDeterministicVictim)
{
    ErrorContext ctx;

    uint32_t proc1 = 0;
    uint32_t proc2 = 0;
    ASSERT_EQ(scratchbird::core::ProcArrayManager::registerBackend(&proc1, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(scratchbird::core::ProcArrayManager::registerBackend(&proc2, &ctx), Status::OK)
        << ctx.message;

    uint64_t xid1 = 0;
    uint64_t xid2 = 0;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc1, xid1, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(txn_mgr_->beginTransaction(proc2, xid2, &ctx), Status::OK) << ctx.message;

    const LockTag tag_a = makePageLockTag(100);
    const LockTag tag_b = makePageLockTag(200);

    std::atomic<int> stage1{0};
    std::atomic<int> stage2{0};
    std::atomic<Status> t1_status{Status::OK};
    std::atomic<Status> t2_status{Status::OK};

    auto worker = [&](uint32_t proc_id,
                      const LockTag& first,
                      const LockTag& second,
                      std::atomic<Status>& out) {
        ErrorContext local_ctx;
        Status status = lock_mgr_->acquireLock(proc_id, first, LockMode::LOCK_EXCLUSIVE, false, 0, &local_ctx);
        if (status != Status::OK)
        {
            out.store(status);
            return;
        }

        stage1.fetch_add(1);
        auto start_wait = std::chrono::steady_clock::now();
        while (stage1.load() < 2)
        {
            if (std::chrono::steady_clock::now() - start_wait > std::chrono::seconds(1))
            {
                out.store(Status::LOCK_TIMEOUT);
                lock_mgr_->releaseAllLocks(proc_id, nullptr);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        stage2.fetch_add(1);
        status = lock_mgr_->acquireLock(proc_id,
                                        second,
                                        LockMode::LOCK_EXCLUSIVE,
                                        true,
                                        2000,
                                        &local_ctx);
        out.store(status);
        lock_mgr_->releaseAllLocks(proc_id, nullptr);
    };

    std::thread t1(worker, proc1, tag_a, tag_b, std::ref(t1_status));
    std::thread t2(worker, proc2, tag_b, tag_a, std::ref(t2_status));

    bool deadlock_ready = false;
    auto wait_start = std::chrono::steady_clock::now();
    while (stage2.load() < 2)
    {
        if (std::chrono::steady_clock::now() - wait_start > std::chrono::seconds(2))
        {
            ADD_FAILURE() << "Timed out waiting for deadlock setup";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (stage2.load() >= 2)
    {
        deadlock_ready = true;
    }

    if (deadlock_ready)
    {
        ASSERT_EQ(lock_mgr_->detectDeadlocks(&ctx), Status::OK) << ctx.message;
    }

    t1.join();
    t2.join();

    LockStats stats{};
    lock_mgr_->getStatistics(&stats);
    EXPECT_GE(stats.deadlocks_detected, 1u);

    const Status s1 = t1_status.load();
    const Status s2 = t2_status.load();
    EXPECT_TRUE(s1 == Status::OK || s2 == Status::OK);

    scratchbird::core::ProcArrayManager::unregisterBackend(proc1, &ctx);
    scratchbird::core::ProcArrayManager::unregisterBackend(proc2, &ctx);
}

class SweepChaosTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("reliability_sweep_chaos", ".sbdb");
    }

    void TearDown() override
    {
        db_file_.reset();
    }

    auto createTestDatabase(Database& db) -> bool
    {
        ErrorContext ctx;
        return Database::create(db_file_->path(), 16384, &ctx) == Status::OK &&
               db.open(db_file_->path(), &ctx) == Status::OK &&
               db.garbage_collector() != nullptr &&
               db.garbage_collector()->initialize(&ctx) == Status::OK;
    }

    auto createCommittedRetainedTransaction(Database& db, ID& tx_uuid_out, uint64_t& txid_out) -> bool
    {
        ErrorContext ctx;
        std::unique_ptr<ConnectionContext> conn;
        if (db.connect(conn, &ctx) != Status::OK)
        {
            return false;
        }

        txid_out = conn->getCurrentXid();
        tx_uuid_out = conn->getCurrentTransactionUuid();
        if (txid_out == 0 || tx_uuid_out == ID{})
        {
            return false;
        }
        return conn->commit(&ctx) == Status::OK;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
};

TEST_F(SweepChaosTest, SweepEvidencePersistenceFailureBlocksPruneDeterministically)
{
    Database db;
    ASSERT_TRUE(createTestDatabase(db));

    auto* sweep_mgr = db.sweep_manager();
    auto* gc = db.garbage_collector();
    ASSERT_NE(sweep_mgr, nullptr);
    ASSERT_NE(gc, nullptr);

    ID tx_uuid{};
    uint64_t txid = 0;
    ASSERT_TRUE(createCommittedRetainedTransaction(db, tx_uuid, txid));

    SweepPolicyBinding binding{};
    binding.scope_kind = SweepScopeKind::DATABASE;
    binding.scope_id = db.uuid();
    binding.lanes = {SweepPolicyLane::LINEAGE_RETENTION};
    binding.strict_audit = true;

    ErrorContext ctx;
    ASSERT_EQ(sweep_mgr->setPolicyBindings({binding}, &ctx), Status::OK) << ctx.message;

    std::filesystem::path blocking_path(db.path());
    blocking_path += ".forensics";
    {
        std::ofstream out(blocking_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << "blocked";
    }

    EXPECT_EQ(sweep_mgr->executeSweep(false, &ctx), Status::IO_ERROR);
    EXPECT_TRUE(gc->isSweepPruneBlocked());

    auto stats = sweep_mgr->getStatistics();
    EXPECT_EQ(stats.evidence_persist_failures, 1u);
    EXPECT_TRUE(stats.prune_blocked);

    std::vector<scratchbird::core::SweepEvidenceWorkItem> items;
    ASSERT_EQ(sweep_mgr->listEvidenceWorkItems(items, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(std::none_of(items.begin(), items.end(), [txid](const auto& item) {
        return item.txid == txid;
    }));
}

class IPCChaosTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        if (!db_name_.empty())
        {
            std::string socket_path = scratchbird::server::getIPCPath(db_name_, IPCMethod::UNIX_SOCKET);
            ::unlink(socket_path.c_str());
        }
    }

    std::string db_name_;
};

TEST_F(IPCChaosTest, TcpConnectionFailureRecoversAfterServerStarts)
{
    const uint16_t tcp_port = reserveTcpPort();
    ASSERT_NE(tcp_port, 0u);

    ErrorContext ctx;
    IPCClientConfig client_config("reliability_tcp_chaos", IPCMethod::TCP_LOCALHOST);
    client_config.tcp_port = tcp_port;
    client_config.connect_timeout_ms = 200;

    auto client = IPCClient::create(client_config, &ctx);
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->connect(&ctx), Status::CONNECTION_FAILURE);

    IPCServerConfig server_config("reliability_tcp_chaos", IPCMethod::TCP_LOCALHOST);
    server_config.tcp_port = tcp_port;
    server_config.accept_timeout_ms = 1000;

    auto server = IPCServer::create(server_config, &ctx);
    ASSERT_NE(server, nullptr);
    ASSERT_EQ(server->listen(&ctx), Status::OK) << ctx.message;

    std::unique_ptr<IPCConnection> accepted;
    std::thread accept_thread([&]() {
        ErrorContext accept_ctx;
        accepted = server->accept(&accept_ctx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(client->connect(&ctx), Status::OK) << ctx.message;

    accept_thread.join();
    ASSERT_NE(accepted, nullptr);
    EXPECT_TRUE(client->isConnected());

    client->disconnect();
    accepted->close();
    server->close();
}

TEST_F(IPCChaosTest, UnixSocketFailureRecoversAfterServerStarts)
{
    db_name_ = "ipc_chaos_" + std::to_string(::getpid()) + "_" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    ErrorContext ctx;
    IPCClientConfig client_config(db_name_, IPCMethod::UNIX_SOCKET);
    client_config.connect_timeout_ms = 200;
    auto client = IPCClient::create(client_config, &ctx);
    ASSERT_NE(client, nullptr);

    Status initial_status = client->connect(&ctx);
    if (initial_status != Status::CONNECTION_FAILURE && isNetworkRestrictedError(ctx))
    {
        GTEST_SKIP() << "Unix sockets not permitted in this environment: " << ctx.message;
    }
    EXPECT_EQ(initial_status, Status::CONNECTION_FAILURE);

    IPCServerConfig server_config(db_name_, IPCMethod::UNIX_SOCKET);
    server_config.accept_timeout_ms = 1000;
    auto server = IPCServer::create(server_config, &ctx);
    ASSERT_NE(server, nullptr);

    Status listen_status = server->listen(&ctx);
    if (listen_status != Status::OK && isNetworkRestrictedError(ctx))
    {
        GTEST_SKIP() << "Unix sockets not permitted in this environment: " << ctx.message;
    }
    ASSERT_EQ(listen_status, Status::OK) << ctx.message;

    std::unique_ptr<IPCConnection> accepted;
    std::thread accept_thread([&]() {
        ErrorContext accept_ctx;
        accepted = server->accept(&accept_ctx);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(client->connect(&ctx), Status::OK) << ctx.message;

    accept_thread.join();
    ASSERT_NE(accepted, nullptr);

    client->disconnect();
    accepted->close();
    server->close();
}

} // namespace
