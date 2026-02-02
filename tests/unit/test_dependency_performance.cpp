/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace scratchbird::core;

class DependencyPerformanceTest : public ::testing::Test
{
protected:
    std::string test_db_path;
    Database *db = nullptr;
    CatalogManager *catalog = nullptr;

    void SetUp() override
    {
        test_db_path = "/tmp/test_dep_perf_" + std::to_string(getpid()) + ".sbdb";
        std::remove(test_db_path.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(test_db_path, 16384, &ctx), Status::OK);

        db = new Database();
        ASSERT_EQ(db->open(test_db_path, &ctx), Status::OK);
        catalog = db->catalog_manager();
        ASSERT_NE(catalog, nullptr);
    }

    void TearDown() override
    {
        if (db)
        {
            db->close();
            delete db;
            db = nullptr;
        }
        std::remove(test_db_path.c_str());
        std::remove((test_db_path + "-lock").c_str());
    }

    void createDependencies(const ID& referenced_id, size_t count)
    {
        ErrorContext ctx;
        for (size_t i = 0; i < count; ++i)
        {
            ID dep_id;
            ASSERT_EQ(catalog->createDependency(
                generateUuidV7(), CatalogManager::ObjectType::VIEW,
                referenced_id, CatalogManager::ObjectType::TABLE,
                CatalogManager::DependencyType::NORMAL, dep_id, &ctx), Status::OK);
        }
    }
};

TEST_F(DependencyPerformanceTest, GetDependents1K)
{
    ID table_id = generateUuidV7();
    createDependencies(table_id, 1000);

    ErrorContext ctx;
    std::vector<CatalogManager::DependencyInfo> deps;
    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(catalog->getDependents(table_id, deps, &ctx), Status::OK);
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(deps.size(), 1000);
    EXPECT_LT(ms, 10) << "getDependents(1K) took " << ms << "ms";
}

TEST_F(DependencyPerformanceTest, GetDependents10K)
{
    ID table_id = generateUuidV7();
    createDependencies(table_id, 10000);

    ErrorContext ctx;
    std::vector<CatalogManager::DependencyInfo> deps;
    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(catalog->getDependents(table_id, deps, &ctx), Status::OK);
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(deps.size(), 10000);
    EXPECT_LT(ms, 50) << "getDependents(10K) took " << ms << "ms";
}

TEST_F(DependencyPerformanceTest, GetDependents100K)
{
    ID table_id = generateUuidV7();
    createDependencies(table_id, 100000);

    ErrorContext ctx;
    std::vector<CatalogManager::DependencyInfo> deps;
    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(catalog->getDependents(table_id, deps, &ctx), Status::OK);
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(deps.size(), 100000);
    EXPECT_LT(ms, 200) << "getDependents(100K) took " << ms << "ms";
}
