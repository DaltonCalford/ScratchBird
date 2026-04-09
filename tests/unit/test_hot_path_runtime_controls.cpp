#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/index_factory.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"

namespace {

using Config = scratchbird::core::Config;
using Database = scratchbird::core::Database;
using ErrorContext = scratchbird::core::ErrorContext;
using ID = scratchbird::core::ID;
using IndexFactory = scratchbird::core::IndexFactory;
using IndexType = scratchbird::core::CatalogManager::IndexType;
using LSMTreeIndex = scratchbird::core::LSMTreeIndex;
using Status = scratchbird::core::Status;

std::atomic<uint64_t> g_temp_counter{0};

std::filesystem::path makeUniqueTempRoot(const std::string& prefix)
{
    const auto stamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const uint64_t ordinal = g_temp_counter.fetch_add(1, std::memory_order_relaxed);
    return std::filesystem::temp_directory_path() /
           (prefix + "_" + std::to_string(stamp) + "_" + std::to_string(ordinal));
}

void writeTextFile(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream out(path);
    ASSERT_TRUE(out.is_open()) << path;
    out << contents;
}

class EnvVarGuard
{
public:
    EnvVarGuard(std::string name, std::string value)
        : name_(std::move(name))
    {
        const char* existing = std::getenv(name_.c_str());
        if (existing != nullptr)
        {
            had_original_ = true;
            original_value_ = existing;
        }
#if defined(_WIN32)
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    ~EnvVarGuard()
    {
        if (had_original_)
        {
#if defined(_WIN32)
            _putenv_s(name_.c_str(), original_value_.c_str());
#else
            setenv(name_.c_str(), original_value_.c_str(), 1);
#endif
        }
        else
        {
#if defined(_WIN32)
            _putenv_s(name_.c_str(), "");
#else
            unsetenv(name_.c_str());
#endif
        }
    }

private:
    std::string name_;
    std::string original_value_;
    bool had_original_ = false;
};

class HotPathRuntimeFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Config::getInstance().clear();
        temp_root_ = makeUniqueTempRoot("scratchbird_hot_path_runtime");
        std::filesystem::create_directories(temp_root_);
    }

    void TearDown() override
    {
        Config::getInstance().clear();
        std::error_code ec;
        std::filesystem::remove_all(temp_root_, ec);
    }

    auto configPath() const -> std::filesystem::path
    {
        return temp_root_ / "sb_config.ini";
    }

    auto dbPath() const -> std::filesystem::path
    {
        return temp_root_ / "hot_path.sbdb";
    }

    void initializeConfig(const std::string& body)
    {
        writeTextFile(configPath(), body);
        ErrorContext ctx;
        ASSERT_EQ(Config::getInstance().initialize(configPath().string(), &ctx), Status::OK)
            << ctx.message;
    }

    void createDatabase(uint32_t page_size = 16384)
    {
        ErrorContext ctx;
        ASSERT_EQ(Database::create(dbPath().string(), page_size, &ctx), Status::OK) << ctx.message;
    }

    std::filesystem::path temp_root_;
};

TEST_F(HotPathRuntimeFixture, DatabaseOpenLoadsSupportedBufferPoolControls)
{
    initializeConfig(
        "[memory]\n"
        "buffer_pool_size = 64KB\n"
        "buffer_pool_layout = single\n"
        "buffer_pool_bgwriter_enabled = false\n"
        "buffer_pool_bgwriter_max_pages = 7\n"
        "buffer_pool_dirty_ratio_low = 0.10\n"
        "buffer_pool_dirty_ratio_high = 0.20\n"
        "buffer_pool_dirty_ratio_checkpoint = 0.30\n");
    createDatabase();

    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(dbPath().string(), &ctx), Status::OK) << ctx.message;
    ASSERT_NE(db.buffer_pool(), nullptr);

    const auto pool_config = db.buffer_pool()->getConfigSnapshot();
    EXPECT_EQ(pool_config.pool_size, 4U);
    EXPECT_EQ(pool_config.page_size, 16384U);
    EXPECT_EQ(pool_config.layout, scratchbird::core::BufferPool::PoolLayout::Single);
    EXPECT_FALSE(pool_config.enable_background_writer);
    EXPECT_EQ(pool_config.bgwriter_max_pages, 7U);
    EXPECT_DOUBLE_EQ(pool_config.dirty_ratio_low, 0.10);
    EXPECT_DOUBLE_EQ(pool_config.dirty_ratio_high, 0.20);
    EXPECT_DOUBLE_EQ(pool_config.dirty_ratio_checkpoint, 0.30);

    db.close();
}

TEST_F(HotPathRuntimeFixture, DatabaseOpenLoadsCanonicalSegmentedBufferDomainControls)
{
    initializeConfig(
        "[storage.buffer]\n"
        "profile = analytics\n"
        "layout = segmented\n"
        "pool_size_mb = 1\n"
        "[storage.buffer.domain.critical_system]\n"
        "min_pct = 9\n"
        "[storage.buffer.domain.hot_oltp]\n"
        "target_pct = 27\n"
        "[storage.buffer.domain.read_mostly]\n"
        "target_pct = 31\n"
        "[storage.buffer.domain.scan_bulk_ring]\n"
        "max_pct = 14\n"
        "[storage.buffer.domain.version_undo]\n"
        "min_pct = 17\n"
        "[storage.buffer.domain.temporary_work]\n"
        "max_pct = 8\n"
        "[storage.buffer.replacement]\n"
        "protected_pct = 42\n"
        "ghost_history_pct = 18\n"
        "[storage.buffer.admission]\n"
        "second_touch_generations = 4\n"
        "direct_protect_roots = false\n"
        "[storage.buffer.prefetch]\n"
        "enabled = true\n"
        "workers = 3\n"
        "scan_window_pages = 12\n"
        "index_window_pages = 6\n"
        "chain_window_pages = 5\n"
        "max_debt_pages = 19\n"
        "usefulness_floor_pct = 65\n"
        "[storage.buffer.thrash]\n"
        "session_budget_pct = 14\n"
        "object_budget_pct = 21\n"
        "prefetch_pressure_pct = 77\n"
        "[storage.buffer.writeback]\n"
        "enabled = false\n"
        "batch_pages = 11\n"
        "low_dirty_pct = 12\n"
        "high_dirty_pct = 24\n"
        "checkpoint_target_pct = 48\n");
    createDatabase();

    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(dbPath().string(), &ctx), Status::OK) << ctx.message;
    ASSERT_NE(db.buffer_pool(), nullptr);

    const auto pool_config = db.buffer_pool()->getConfigSnapshot();
    EXPECT_EQ(pool_config.profile, scratchbird::core::BufferPool::BufferProfile::Analytics);
    EXPECT_EQ(pool_config.pool_size, 64U);
    EXPECT_EQ(pool_config.page_size, 16384U);
    EXPECT_EQ(pool_config.layout, scratchbird::core::BufferPool::PoolLayout::Segmented);
    EXPECT_FALSE(pool_config.enable_background_writer);
    EXPECT_EQ(pool_config.bgwriter_max_pages, 11U);
    EXPECT_DOUBLE_EQ(pool_config.dirty_ratio_low, 0.12);
    EXPECT_DOUBLE_EQ(pool_config.dirty_ratio_high, 0.24);
    EXPECT_DOUBLE_EQ(pool_config.dirty_ratio_checkpoint, 0.48);
    EXPECT_EQ(pool_config.replacement_protected_pct, 42U);
    EXPECT_EQ(pool_config.replacement_ghost_history_pct, 18U);
    EXPECT_EQ(pool_config.admission_second_touch_generations, 4U);
    EXPECT_FALSE(pool_config.admission_direct_protect_roots);
    EXPECT_TRUE(pool_config.prefetch_enabled);
    EXPECT_EQ(pool_config.prefetch_workers, 3U);
    EXPECT_EQ(pool_config.prefetch_scan_window_pages, 12U);
    EXPECT_EQ(pool_config.prefetch_index_window_pages, 6U);
    EXPECT_EQ(pool_config.prefetch_chain_window_pages, 5U);
    EXPECT_EQ(pool_config.prefetch_max_debt_pages, 19U);
    EXPECT_EQ(pool_config.prefetch_usefulness_floor_pct, 65U);
    EXPECT_EQ(pool_config.thrash_session_budget_pct, 14U);
    EXPECT_EQ(pool_config.thrash_object_budget_pct, 21U);
    EXPECT_EQ(pool_config.thrash_prefetch_pressure_pct, 77U);

    const auto critical_budget =
        pool_config.domainBudget(scratchbird::core::BufferPool::PolicyDomain::CriticalSystem);
    const auto hot_budget =
        pool_config.domainBudget(scratchbird::core::BufferPool::PolicyDomain::HotOltp);
    const auto read_budget =
        pool_config.domainBudget(scratchbird::core::BufferPool::PolicyDomain::ReadMostly);
    const auto scan_budget =
        pool_config.domainBudget(scratchbird::core::BufferPool::PolicyDomain::ScanBulkRing);
    const auto version_budget =
        pool_config.domainBudget(scratchbird::core::BufferPool::PolicyDomain::VersionUndo);
    const auto temp_budget =
        pool_config.domainBudget(scratchbird::core::BufferPool::PolicyDomain::TemporaryWork);

    EXPECT_DOUBLE_EQ(critical_budget.min_pct, 9.0);
    EXPECT_EQ(critical_budget.min_frames, 6U);
    EXPECT_DOUBLE_EQ(hot_budget.target_pct, 27.0);
    EXPECT_EQ(hot_budget.target_frames, 18U);
    EXPECT_DOUBLE_EQ(read_budget.target_pct, 31.0);
    EXPECT_EQ(read_budget.target_frames, 20U);
    EXPECT_DOUBLE_EQ(scan_budget.max_pct, 14.0);
    EXPECT_EQ(scan_budget.max_frames, 9U);
    EXPECT_DOUBLE_EQ(version_budget.min_pct, 17.0);
    EXPECT_EQ(version_budget.min_frames, 11U);
    EXPECT_DOUBLE_EQ(temp_budget.max_pct, 8.0);
    EXPECT_EQ(temp_budget.max_frames, 6U);

    db.close();
}

TEST_F(HotPathRuntimeFixture, DatabaseOpenRejectsUnsupportedBufferPoolLayouts)
{
    initializeConfig(
        "[memory]\n"
        "buffer_pool_size = 8\n"
        "buffer_pool_layout = hot_cold\n");
    createDatabase();

    Database db;
    ErrorContext ctx;
    EXPECT_EQ(db.open(dbPath().string(), &ctx), Status::NOT_IMPLEMENTED);
    EXPECT_NE(ctx.message.find("canonical 'segmented' and legacy 'single'"), std::string::npos)
        << ctx.message;
}

TEST_F(HotPathRuntimeFixture, DatabaseOpenRejectsIndependentBufferPoolPageSizeOverrides)
{
    initializeConfig(
        "[memory]\n"
        "buffer_pool_size = 8\n"
        "buffer_pool_page_size = 8192\n");
    createDatabase();

    Database db;
    ErrorContext ctx;
    EXPECT_EQ(db.open(dbPath().string(), &ctx), Status::INVALID_ARGUMENT);
    EXPECT_NE(ctx.message.find("fixed by the database page size"), std::string::npos)
        << ctx.message;
}

TEST_F(HotPathRuntimeFixture, DatabaseOpenRejectsDomainMinimumsThatEliminateSharedCapacity)
{
    initializeConfig(
        "[storage.buffer]\n"
        "layout = segmented\n"
        "pool_size_mb = 1\n"
        "[storage.buffer.domain.critical_system]\n"
        "min_pct = 50\n"
        "[storage.buffer.domain.version_undo]\n"
        "min_pct = 50\n");
    createDatabase();

    Database db;
    ErrorContext ctx;
    EXPECT_EQ(db.open(dbPath().string(), &ctx), Status::INVALID_ARGUMENT);
    EXPECT_NE(ctx.message.find("no shared probationary capacity"), std::string::npos)
        << ctx.message;
}

TEST_F(HotPathRuntimeFixture, DatabaseOpenClampsExplicitBufferPoolToDetectedMemoryCeiling)
{
    EnvVarGuard memory_limit("SCRATCHBIRD_TEST_CGROUP_MEMORY_LIMIT_BYTES", "131072");

    initializeConfig(
        "[storage.buffer]\n"
        "layout = segmented\n"
        "pool_size_mb = 1\n");
    createDatabase();

    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(dbPath().string(), &ctx), Status::OK) << ctx.message;
    ASSERT_NE(db.buffer_pool(), nullptr);

    const auto pool_config = db.buffer_pool()->getConfigSnapshot();
    EXPECT_EQ(pool_config.pool_size, 8U);
    EXPECT_EQ(pool_config.configured_memory_request_bytes, 1048576ULL);
    EXPECT_EQ(pool_config.detected_memory_ceiling_bytes, 131072ULL);
    EXPECT_EQ(pool_config.effective_memory_budget_bytes, 131072ULL);
    EXPECT_TRUE(pool_config.memory_budget_clamped);
    EXPECT_TRUE(pool_config.memory_ceiling_is_environment_bounded);

    db.close();
}

TEST_F(HotPathRuntimeFixture, DatabaseOpenDerivesImplicitBufferPoolFromDetectedMemoryCeiling)
{
    EnvVarGuard memory_limit("SCRATCHBIRD_TEST_CGROUP_MEMORY_LIMIT_BYTES", "2147483648");

    initializeConfig("");
    createDatabase();

    Database db;
    ErrorContext ctx;
    ASSERT_EQ(db.open(dbPath().string(), &ctx), Status::OK) << ctx.message;
    ASSERT_NE(db.buffer_pool(), nullptr);

    const auto pool_config = db.buffer_pool()->getConfigSnapshot();
    EXPECT_EQ(pool_config.pool_size, 16384U);
    EXPECT_EQ(pool_config.configured_memory_request_bytes, 268435456ULL);
    EXPECT_EQ(pool_config.detected_memory_ceiling_bytes, 2147483648ULL);
    EXPECT_EQ(pool_config.effective_memory_budget_bytes, 268435456ULL);
    EXPECT_FALSE(pool_config.memory_budget_clamped);
    EXPECT_TRUE(pool_config.memory_ceiling_is_environment_bounded);

    db.close();
}

class HotPathLsmRuntimeTest : public HotPathRuntimeFixture
{
protected:
    void SetUp() override
    {
        HotPathRuntimeFixture::SetUp();
        createDatabase();

        db_ = std::make_unique<Database>();
        ErrorContext ctx;
        ASSERT_EQ(db_->open(dbPath().string(), &ctx), Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        if (db_ != nullptr)
        {
            db_->close();
            db_.reset();
        }
        HotPathRuntimeFixture::TearDown();
    }

    std::unique_ptr<Database> db_;
};

TEST_F(HotPathLsmRuntimeTest, LsmIndexCreateWiresConfiguredRuntimePathAndBlockSize)
{
    const ID index_id = scratchbird::core::generateUuidV7();
    const std::filesystem::path index_path =
        IndexFactory::generateIndexPath(db_->path(), index_id, IndexType::LSM);
    std::filesystem::create_directories(index_path.parent_path());

    LSMTreeIndex index(db_.get(), index_path.string(), db_->transaction_manager(), 2);
    ErrorContext ctx;
    ASSERT_EQ(index.create(&ctx), Status::OK) << ctx.message;
    ASSERT_NE(index.compactionManager(), nullptr);

    EXPECT_EQ(index.indexPath(), index_path.string());
    EXPECT_EQ(index.memtableMaxSizeBytes(), 2U * 1024U * 1024U);
    EXPECT_EQ(index.blockSize(), db_->page_size());
    EXPECT_EQ(index.compactionManager()->indexPath(), index_path.string());
    EXPECT_EQ(index.compactionManager()->blockSize(), db_->page_size());
    EXPECT_TRUE(std::filesystem::exists(index_path));

    ASSERT_EQ(index.close(&ctx), Status::OK) << ctx.message;
}

TEST_F(HotPathLsmRuntimeTest, LsmStaticOpenReusesFactoryGeneratedIndexPath)
{
    const ID index_id = scratchbird::core::generateUuidV7();
    const std::filesystem::path index_path =
        IndexFactory::generateIndexPath(db_->path(), index_id, IndexType::LSM);
    std::filesystem::create_directories(index_path.parent_path());

    ErrorContext ctx;
    {
        LSMTreeIndex created(db_.get(), index_path.string(), db_->transaction_manager(), 1);
        ASSERT_EQ(created.create(&ctx), Status::OK) << ctx.message;
        ASSERT_EQ(created.close(&ctx), Status::OK) << ctx.message;
    }

    auto reopened = LSMTreeIndex::open(db_.get(), index_id, 0, &ctx);
    ASSERT_NE(reopened, nullptr) << ctx.message;
    EXPECT_EQ(reopened->indexPath(), index_path.string());
    EXPECT_EQ(reopened->blockSize(), db_->page_size());

    ASSERT_EQ(reopened->close(&ctx), Status::OK) << ctx.message;
}

}  // namespace
