/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Standalone test for Issue 1.7 overflow fix
 * Verifies that overflow checks happen BEFORE calculations
 */

#include "include/scratchbird/core/page_manager.h"
#include "include/scratchbird/core/database.h"
#include "test_helpers.h"
#include <iostream>
#include <cstdio>
#include <limits>

using namespace scratchbird::core;
using scratchbird::testing::uniqueTestShortPath;

int main()
{
    std::cout << "=== Testing Issue 1.7: Integer Overflow in Bitmap Extension ===" << std::endl;
    std::cout << std::endl;

    // Test 1: Normal extension works
    {
        std::cout << "Test 1: Normal extension... ";
        std::string db_path = uniqueTestShortPath("test_overflow_1", ".sbrd");
        std::remove(db_path.c_str());

        ErrorContext ctx;
        if (Database::create(db_path.c_str(), 8192, &ctx) != Status::OK)
        {
            std::cout << "FAILED (create): " << ctx.message << std::endl;
            return 1;
        }

        Database db;
        if (db.open(db_path.c_str(), &ctx) != Status::OK)
        {
            std::cout << "FAILED (open): " << ctx.message << std::endl;
            return 1;
        }

        PageManager *pm = db.page_manager();
        uint32_t initial_pages = pm->totalPages();
        std::cout << "(initial=" << initial_pages << ") ";

        if (pm->extendFile(1000, &ctx) != Status::OK)
        {
            std::cout << "FAILED (extend): " << ctx.message << std::endl;
            return 1;
        }

        uint32_t expected_pages = initial_pages + 1000;
        if (pm->totalPages() != expected_pages)
        {
            std::cout << "FAILED (count): expected " << expected_pages << ", got " << pm->totalPages() << std::endl;
            return 1;
        }

        db.close();
        std::remove(db_path.c_str());
        std::cout << "PASSED" << std::endl;
    }

    // Test 2: Huge extension is rejected (overflow protection)
    {
        std::cout << "Test 2: Overflow detection... ";
        std::string db_path = uniqueTestShortPath("test_overflow_2", ".sbrd");
        std::remove(db_path.c_str());

        ErrorContext ctx;
        if (Database::create(db_path.c_str(), 8192, &ctx) != Status::OK)
        {
            std::cout << "FAILED (create): " << ctx.message << std::endl;
            return 1;
        }

        Database db;
        if (db.open(db_path.c_str(), &ctx) != Status::OK)
        {
            std::cout << "FAILED (open): " << ctx.message << std::endl;
            return 1;
        }

        PageManager *pm = db.page_manager();

        // Try to extend by UINT32_MAX - 1000 (should trigger overflow check)
        // extendFile takes uint32_t, so use that limit
        constexpr uint32_t huge_extension = UINT32_MAX - 1000;
        Status status = pm->extendFile(huge_extension, &ctx);

        if (status == Status::OK)
        {
            std::cout << "FAILED: Should have rejected huge extension" << std::endl;
            return 1;
        }

        if (status != Status::OOM)
        {
            std::cout << "FAILED: Should return OOM, got status " << static_cast<int>(status) << std::endl;
            return 1;
        }

        // Verify error message mentions addressable space
        std::string msg(ctx.message);
        if (msg.find("addressable space") == std::string::npos &&
            msg.find("exceed") == std::string::npos)
        {
            std::cout << "FAILED: Error message should mention overflow/addressable space, got: "
                      << ctx.message << std::endl;
            return 1;
        }

        db.close();
        std::remove(db_path.c_str());
        std::cout << "PASSED" << std::endl;
    }

    // Test 3: Multiple small extensions work correctly
    {
        std::cout << "Test 3: Multiple extensions... ";
        std::string db_path = uniqueTestShortPath("test_overflow_3", ".sbrd");
        std::remove(db_path.c_str());

        ErrorContext ctx;
        if (Database::create(db_path.c_str(), 8192, &ctx) != Status::OK)
        {
            std::cout << "FAILED (create): " << ctx.message << std::endl;
            return 1;
        }

        Database db;
        if (db.open(db_path.c_str(), &ctx) != Status::OK)
        {
            std::cout << "FAILED (open): " << ctx.message << std::endl;
            return 1;
        }

        PageManager *pm = db.page_manager();
        uint32_t initial_pages = pm->totalPages();

        // Extend 100 times by 100 pages
        for (int i = 0; i < 100; i++)
        {
            if (pm->extendFile(100, &ctx) != Status::OK)
            {
                std::cout << "FAILED (extend " << i << "): " << ctx.message << std::endl;
                return 1;
            }
        }

        uint32_t expected_pages = initial_pages + 10000;
        if (pm->totalPages() != expected_pages)
        {
            std::cout << "FAILED (count): expected " << expected_pages << ", got " << pm->totalPages() << std::endl;
            return 1;
        }

        db.close();
        std::remove(db_path.c_str());
        std::cout << "PASSED" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=== All tests PASSED ===" << std::endl;
    std::cout << "Issue 1.7 overflow protection is working correctly!" << std::endl;
    std::cout << std::endl;
    std::cout << "Fix summary:" << std::endl;
    std::cout << "  - Overflow check moved BEFORE addition (line 246)" << std::endl;
    std::cout << "  - Prevents undefined behavior from num_pages + total_pages_ overflow" << std::endl;
    std::cout << "  - Second check ensures bitmap calculation won't overflow (line 256)" << std::endl;

    return 0;
}
