#include <gtest/gtest.h>

#include "scratchbird/core/database.h"
#include "scratchbird/core/inverted_index.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/uuidv7.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#ifndef _WIN32
#include <dlfcn.h>
#endif

using namespace scratchbird::core;

namespace {

bool hasSnowballStemmer()
{
#ifdef _WIN32
    return false;
#else
    const char* candidates[] = {"libstemmer.so", "libstemmer.so.0"};
    for (const char* name : candidates)
    {
        void* handle = dlopen(name, RTLD_LAZY);
        if (!handle)
        {
            continue;
        }
        auto stemmer_new = dlsym(handle, "sb_stemmer_new");
        auto stemmer_stem = dlsym(handle, "sb_stemmer_stem");
        auto stemmer_len = dlsym(handle, "sb_stemmer_length");
        auto stemmer_del = dlsym(handle, "sb_stemmer_delete");
        dlclose(handle);
        if (stemmer_new && stemmer_stem && stemmer_len && stemmer_del)
        {
            return true;
        }
    }
    return false;
#endif
}

class InvertedIndexBasicTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        test_db_path_ = "/tmp/test_inverted_index_basic.db";
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }

        db_ = std::make_unique<Database>();
        Status status = db_->create(test_db_path_, 16384, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to create database";

        status = db_->open(test_db_path_, nullptr);
        ASSERT_EQ(status, Status::OK) << "Failed to open database";
    }

    void TearDown() override
    {
        if (db_)
        {
            db_->close();
            db_.reset();
        }
        if (std::filesystem::exists(test_db_path_))
        {
            std::filesystem::remove(test_db_path_);
        }
    }

    std::unique_ptr<Database> db_;
    std::string test_db_path_;
};

TEST_F(InvertedIndexBasicTest, InsertSearchRemove)
{
    ID index_uuid = generateUuidV7();
    ID table_uuid = generateUuidV7();
    ID column_uuid = generateUuidV7();

    GPID meta_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    InvertedIndexConfig config;
    config.features = II_FEATURE_STOP_WORDS | II_FEATURE_POSITIONS |
        II_FEATURE_OFFSETS | II_FEATURE_PAYLOADS;

    status = InvertedIndex::create(db_.get(), index_uuid, table_uuid, column_uuid,
                                   meta_gpid, config, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto index = InvertedIndex::open(db_.get(), index_uuid, table_uuid, column_uuid,
                                     meta_gpid, nullptr);
    ASSERT_NE(index, nullptr);

    const std::string doc1 = "the quick brown fox";
    const std::string doc2 = "quick brown dog";

    TID tid1(0, 10, 1);
    TID tid2(0, 11, 1);

    status = index->insert(doc1.data(), doc1.size(), tid1, nullptr);
    ASSERT_EQ(status, Status::OK);
    status = index->insert(doc2.data(), doc2.size(), tid2, nullptr);
    ASSERT_EQ(status, Status::OK);

    index.reset();
    index = InvertedIndex::open(db_.get(), index_uuid, table_uuid, column_uuid,
                                meta_gpid, nullptr);
    ASSERT_NE(index, nullptr);

    std::vector<TID> results;
    status = index->search("quick", 0, &results, nullptr);
    ASSERT_EQ(status, Status::OK);
    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(std::find(results.begin(), results.end(), tid1) != results.end());
    EXPECT_TRUE(std::find(results.begin(), results.end(), tid2) != results.end());

    results.clear();
    status = index->search("fox", 0, &results, nullptr);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], tid1);

    results.clear();
    status = index->search("quick & fox", 0, &results, nullptr);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], tid1);

    results.clear();
    status = index->search("quick & !fox", 0, &results, nullptr);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], tid2);

    results.clear();
    status = index->search("brown <1> fox", 0, &results, nullptr);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], tid1);

    status = index->remove(doc1.data(), doc1.size(), tid1, 0, nullptr);
    ASSERT_EQ(status, Status::OK);

    results.clear();
    status = index->search("fox", 0, &results, nullptr);
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(results.empty());
}

TEST_F(InvertedIndexBasicTest, SnowballStemming)
{
    if (!hasSnowballStemmer())
    {
        GTEST_SKIP() << "Snowball stemmer not available";
    }

    ID index_uuid = generateUuidV7();
    ID table_uuid = generateUuidV7();
    ID column_uuid = generateUuidV7();

    GPID meta_gpid = 0;
    Status status = db_->page_manager()->allocatePageInTablespace(0, &meta_gpid, nullptr);
    ASSERT_EQ(status, Status::OK);

    InvertedIndexConfig config;
    config.features = II_FEATURE_STEMMING;

    status = InvertedIndex::create(db_.get(), index_uuid, table_uuid, column_uuid,
                                   meta_gpid, config, nullptr);
    ASSERT_EQ(status, Status::OK);

    auto index = InvertedIndex::open(db_.get(), index_uuid, table_uuid, column_uuid,
                                     meta_gpid, nullptr);
    ASSERT_NE(index, nullptr);

    const std::string doc1 = "running runner runs";
    TID tid1(0, 12, 1);

    status = index->insert(doc1.data(), doc1.size(), tid1, nullptr);
    ASSERT_EQ(status, Status::OK);

    std::vector<TID> results;
    status = index->search("run", 0, &results, nullptr);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0], tid1);
}

} // namespace
