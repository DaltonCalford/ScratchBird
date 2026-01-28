#include <gtest/gtest.h>
#include <filesystem>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/tablespace.h"

using namespace scratchbird::core;

class TablespaceHeaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = "/tmp/test_tablespace_header.db";
        ts_path_ = "/tmp/test_tablespace_header.sbts";
        std::filesystem::remove(db_path_);
        std::filesystem::remove(ts_path_);
    }

    void TearDown() override
    {
        std::filesystem::remove(db_path_);
        std::filesystem::remove(ts_path_);
    }

    std::string db_path_;
    std::string ts_path_;
};

TEST_F(TablespaceHeaderTest, WritesHeaderV2WithLongName)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    TablespaceConfig config;
    config.autoextend_enabled = true;
    config.autoextend_size_mb = 1;
    config.max_size_mb = 0;
    config.prealloc_pages = 2;

    std::string long_name = "tablespace_name_longer_than_thirty_one_chars";
    Status status = db.page_manager()->createTablespace(2, long_name, ts_path_, config, nullptr);
    ASSERT_EQ(status, Status::OK);
    db.close();

    int fd = ::open(ts_path_.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> buffer(8192, 0);
    ssize_t bytes = ::pread(fd, buffer.data(), buffer.size(), 0);
    ::close(fd);
    ASSERT_EQ(bytes, static_cast<ssize_t>(buffer.size()));

    const auto *header = reinterpret_cast<const TablespaceHeader *>(buffer.data());
    EXPECT_EQ(header->page_header.version, TABLESPACE_HEADER_VERSION_V2);
    EXPECT_EQ(std::string(header->tablespace_name), long_name);
}

TEST_F(TablespaceHeaderTest, TablespaceIdAllocationSkipsReservedOne)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    ErrorContext ctx;
    uint16_t tablespace_id = 0;
    Status status = db.catalog_manager()->createTablespace("ts_id", ts_path_, true, 1, 0, 2,
                                                           tablespace_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_GE(tablespace_id, 2);
    EXPECT_NE(tablespace_id, 1);
}

TEST_F(TablespaceHeaderTest, TablespaceFileRecordsLoadMultipleFiles)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    Database db;
    ASSERT_EQ(db.open(db_path_), Status::OK);

    ErrorContext ctx;
    uint16_t tablespace_id = 0;
    Status status = db.catalog_manager()->createTablespace("ts_files", ts_path_, true, 1, 0, 2,
                                                           tablespace_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::string second_path = "/tmp/test_tablespace_header_extra.sbts";
    std::filesystem::remove(second_path);
    status = db.catalog_manager()->writeTablespaceFileRecord(tablespace_id, 1, second_path,
                                                             0, 0, 0, true, 0, 0, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    status = db.catalog_manager()->readTablespaceFileRecords(&ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    TablespaceInfo info;
    status = db.catalog_manager()->getTablespace(tablespace_id, info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    ASSERT_EQ(info.file_paths.size(), 2u);
    EXPECT_EQ(info.file_paths[1], second_path);

    std::filesystem::remove(second_path);
}

TEST_F(TablespaceHeaderTest, TablespaceFileRecordsPersistAcrossRestart)
{
    ASSERT_EQ(Database::create(db_path_, 8192), Status::OK);

    uint16_t tablespace_id = 0;
    std::string second_path = "/tmp/test_tablespace_header_extra_persist.sbts";
    std::filesystem::remove(second_path);

    {
        Database db;
        ASSERT_EQ(db.open(db_path_), Status::OK);

        ErrorContext ctx;
        Status status = db.catalog_manager()->createTablespace("ts_files_persist", ts_path_,
                                                               true, 1, 0, 2,
                                                               tablespace_id, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        status = db.catalog_manager()->writeTablespaceFileRecord(tablespace_id, 1, second_path,
                                                                 0, 0, 0, true, 0, 0, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;

        db.close();
    }

    {
        Database db;
        ASSERT_EQ(db.open(db_path_), Status::OK);

        ErrorContext ctx;
        TablespaceInfo info;
        Status status = db.catalog_manager()->getTablespace(tablespace_id, info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
        ASSERT_EQ(info.file_paths.size(), 2u);
        EXPECT_EQ(info.file_paths[0], ts_path_);
        EXPECT_EQ(info.file_paths[1], second_path);

        db.close();
    }

    std::filesystem::remove(second_path);
}
