#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
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
    EXPECT_NE(ctx.message.find("only 'single' is implemented"), std::string::npos) << ctx.message;
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
