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
#include <vector>
#include <cstdint>
#include <cstring>

#include "scratchbird/core/ondisk.h"

using namespace scratchbird::core;

TEST(MoreCases, BadChecksumOnSystemCatalog)
{
    uint32_t ps = 8192;
    std::vector<uint8_t> page(ps, 0);
    auto *h = reinterpret_cast<PageHeader *>(page.data());
    h->magic = K_MAGIC_SBRD;
    h->version = 1;
    h->page_type = PAGE_TYPE_SYSTEM_CATALOG;
    h->page_size = ps;
    h->checksum = calculatePageChecksum(page.data(), ps);
    // Tamper to invalidate checksum
    page[ps - 32] ^= 0xAA;
    EXPECT_FALSE(validatePageChecksum(page.data(), ps));
}
