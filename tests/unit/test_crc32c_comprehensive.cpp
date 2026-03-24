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
#include <cstdint>
#include <vector>
#include <cstring>
#include <random>

#include "scratchbird/core/ondisk.h"

using namespace scratchbird::core;

namespace
{

void initializeCanonicalChecksummedPage(std::vector<uint8_t>& page,
                                        uint16_t page_type,
                                        uint64_t page_id)
{
    auto* header = reinterpret_cast<PageHeader*>(page.data());
    header->magic = K_MAGIC_SBRD;
    header->version = 1;
    header->page_type = page_type;
    header->page_size = static_cast<uint32_t>(page.size());
    pageSetLower(*header, sizeof(PageHeader));
    pageSetUpper(*header, static_cast<uint32_t>(page.size()));
    pageSetSpecial(*header, static_cast<uint32_t>(page.size()));
    preparePageForWrite(page.data(), static_cast<uint32_t>(page.size()), page_id);
}

} // namespace

/**
 * Comprehensive CRC32C tests to ensure correctness
 * Addresses CRITICAL issue 1.1 from audit report
 */

// Test known CRC32C test vectors
TEST(CRC32C_Comprehensive, KnownTestVectors)
{
    // Test vector 1: Empty string
    {
        uint32_t crc = crc32cCompute(nullptr, 0, 0xFFFFFFFFu) ^ 0xFFFFFFFFu;
        EXPECT_EQ(crc, 0x00000000u) << "Empty input should produce 0x00000000";
    }

    // Test vector 2: "123456789" -> 0xE3069283
    {
        const uint8_t msg[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        uint32_t crc = 0xFFFFFFFFu;
        crc = crc32cCompute(msg, sizeof(msg), crc);
        crc ^= 0xFFFFFFFFu;
        EXPECT_EQ(crc, 0xE3069283u) << "\"123456789\" should produce 0xE3069283";
    }

    // Test vector 3: Single byte 0x00
    {
        const uint8_t msg[] = {0x00};
        uint32_t crc = 0xFFFFFFFFu;
        crc = crc32cCompute(msg, sizeof(msg), crc);
        crc ^= 0xFFFFFFFFu;
        EXPECT_EQ(crc, 0x527D5351u) << "Single 0x00 byte should produce 0x527D5351";
    }

    // Test vector 4: Single byte 0xFF
    {
        const uint8_t msg[] = {0xFF};
        uint32_t crc = 0xFFFFFFFFu;
        crc = crc32cCompute(msg, sizeof(msg), crc);
        crc ^= 0xFFFFFFFFu;
        EXPECT_EQ(crc, 0xFF000000u) << "Single 0xFF byte should produce 0xFF000000";
    }

    // Test vector 5: All zeros (32 bytes)
    {
        std::vector<uint8_t> msg(32, 0x00);
        uint32_t crc = 0xFFFFFFFFu;
        crc = crc32cCompute(msg.data(), msg.size(), crc);
        crc ^= 0xFFFFFFFFu;
        EXPECT_EQ(crc, 0x8A9136AAu) << "32 zero bytes should produce 0x8A9136AA";
    }

    // Test vector 6: All 0xFF (32 bytes)
    {
        std::vector<uint8_t> msg(32, 0xFF);
        uint32_t crc = 0xFFFFFFFFu;
        crc = crc32cCompute(msg.data(), msg.size(), crc);
        crc ^= 0xFFFFFFFFu;
        EXPECT_EQ(crc, 0x62A8AB43u) << "32 0xFF bytes should produce 0x62A8AB43";
    }
}

// Test that checksum field is properly excluded from page checksum calculation
TEST(CRC32C_Comprehensive, ChecksumFieldExclusion)
{
    constexpr uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page(PAGE_SIZE, 0);

    auto* header = reinterpret_cast<PageHeader*>(page.data());
    header->magic = K_MAGIC_SBRD;
    header->version = 1;
    header->page_type = PAGE_TYPE_HEAP;
    header->page_size = PAGE_SIZE;
    header->lsn = 0x1234567890ABCDEFull;
    header->page_id = 42;
    header->flags = PAGE_FLAG_DIRTY | PAGE_FLAG_CHECKSUM_VALID;
    header->generation = 100;
    pageSetLower(*header, sizeof(PageHeader));
    pageSetUpper(*header, PAGE_SIZE);
    pageSetSpecial(*header, PAGE_SIZE);

    // Calculate checksum with different values in checksum field
    header->checksum = 0xDEADBEEF;
    uint32_t checksum1 = calculatePageChecksum(page.data(), PAGE_SIZE);

    header->checksum = 0x00000000;
    uint32_t checksum2 = calculatePageChecksum(page.data(), PAGE_SIZE);

    header->checksum = 0xFFFFFFFF;
    uint32_t checksum3 = calculatePageChecksum(page.data(), PAGE_SIZE);

    header->checksum = 0x12345678;
    uint32_t checksum4 = calculatePageChecksum(page.data(), PAGE_SIZE);

    // All checksums should be identical regardless of checksum field value
    EXPECT_EQ(checksum1, checksum2) << "Checksum should be same regardless of checksum field value";
    EXPECT_EQ(checksum2, checksum3) << "Checksum should be same regardless of checksum field value";
    EXPECT_EQ(checksum3, checksum4) << "Checksum should be same regardless of checksum field value";
}

// Test canonical checksum split: header checksum excludes its own slot and
// payload checksum covers only bytes after the page header.
TEST(CRC32C_Comprehensive, TwoPassCalculation)
{
    constexpr uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page(PAGE_SIZE, 0);

    for (size_t i = CANONICAL_PAGE_HEADER_BYTES;
         i < CANONICAL_PAGE_HEADER_BYTES + 32;
         ++i) {
        page[i] = static_cast<uint8_t>(i);
    }
    initializeCanonicalChecksummedPage(page, PAGE_TYPE_HEAP, 42u);

    const auto* header = reinterpret_cast<const PageHeader*>(page.data());
    const uint32_t header_checksum = header->header_checksum;
    const uint32_t payload_checksum = header->payload_checksum;

    // Mutating ordinary header bytes must change header CRC only.
    page[5] ^= 0x01;
    EXPECT_NE(calculatePageHeaderChecksum(page.data(), header->header_bytes), header_checksum)
        << "Mutating header bytes must change header CRC";
    EXPECT_EQ(calculatePageChecksum(page.data(), PAGE_SIZE), payload_checksum)
        << "Mutating header bytes must not change payload CRC";
    page[5] ^= 0x01;

    // Mutating the header checksum field itself must not affect recomputation.
    page[0x10] ^= 0xFF;
    EXPECT_EQ(calculatePageHeaderChecksum(page.data(), header->header_bytes), header_checksum)
        << "Header checksum field must be excluded from header CRC";
    EXPECT_EQ(calculatePageChecksum(page.data(), PAGE_SIZE), payload_checksum)
        << "Header checksum field must not affect payload CRC";
    page[0x10] ^= 0xFF;

    // Mutating payload bytes must change payload CRC only.
    page[CANONICAL_PAGE_HEADER_BYTES + 20] ^= 0x01;
    EXPECT_EQ(calculatePageHeaderChecksum(page.data(), header->header_bytes), header_checksum)
        << "Payload bytes must not change header CRC";
    EXPECT_NE(calculatePageChecksum(page.data(), PAGE_SIZE), payload_checksum)
        << "Payload bytes must change payload CRC";
}

// Test validation with correct and incorrect checksums
TEST(CRC32C_Comprehensive, ValidationCorrectness)
{
    for (uint32_t page_size : {8192u, 16384u, 32768u, 65536u, 131072u}) {
        std::vector<uint8_t> page(page_size, 0);

        // Fill page with test data
        for (size_t i = CANONICAL_PAGE_HEADER_BYTES;
             i < std::min(size_t(page_size), size_t(256));
             ++i) {
            page[i] = static_cast<uint8_t>(i);
        }

        initializeCanonicalChecksummedPage(page, PAGE_TYPE_HEAP, 0);
        auto* header = reinterpret_cast<PageHeader*>(page.data());

        // Validation should pass
        EXPECT_TRUE(validatePageChecksum(page.data(), page_size))
            << "Page with correct checksum should validate (page_size=" << page_size << ")";

        // Tamper with data and validation should fail
        page[CANONICAL_PAGE_HEADER_BYTES + 4] ^= 0x01;
        EXPECT_FALSE(validatePageChecksum(page.data(), page_size))
            << "Page with tampered data should not validate (page_size=" << page_size << ")";
        page[CANONICAL_PAGE_HEADER_BYTES + 4] ^= 0x01; // restore

        // Set incorrect checksum and validation should fail
        header->payload_checksum ^= 0x12345678;
        EXPECT_FALSE(validatePageChecksum(page.data(), page_size))
            << "Page with incorrect checksum should not validate (page_size=" << page_size << ")";
    }
}

// Test with all supported page sizes
TEST(CRC32C_Comprehensive, AllPageSizes)
{
    for (uint32_t page_size : {8192u, 16384u, 32768u, 65536u, 131072u}) {
        std::vector<uint8_t> page(page_size, 0);

        // Fill with random data
        std::mt19937 rng(42); // Fixed seed for reproducibility
        std::uniform_int_distribution<uint32_t> dist(0, 255);
        for (size_t i = CANONICAL_PAGE_HEADER_BYTES; i < page_size; i++) {
            page[i] = static_cast<uint8_t>(dist(rng));
        }

        initializeCanonicalChecksummedPage(page, PAGE_TYPE_DATABASE_HEADER, 0);

        // Validate
        EXPECT_TRUE(validatePageChecksum(page.data(), page_size))
            << "Page validation failed for page_size=" << page_size;
    }
}

// Test that initial and final XOR are correctly applied
TEST(CRC32C_Comprehensive, InitialAndFinalXOR)
{
    const uint8_t msg[] = {0x01, 0x02, 0x03, 0x04};

    // Without initial/final XOR (incorrect)
    uint32_t crc_no_xor = crc32cCompute(msg, sizeof(msg), 0);

    // With initial/final XOR (correct per spec)
    uint32_t crc_with_xor = crc32cCompute(msg, sizeof(msg), 0xFFFFFFFFu) ^ 0xFFFFFFFFu;

    // These should be different
    EXPECT_NE(crc_no_xor, crc_with_xor) << "Initial/final XOR should affect result";

    // The correct CRC32C (Castagnoli) spec-compliant value for {0x01,0x02,0x03,0x04}
    // Verified against standard test vector "123456789" -> 0xE3069283
    EXPECT_EQ(crc_with_xor, 0x29308CF4U) << "CRC32C of {0x01,0x02,0x03,0x04} should be 0x29308CF4";
}

// Test incremental checksum calculation (two-part computation)
TEST(CRC32C_Comprehensive, IncrementalCalculation)
{
    std::vector<uint8_t> data(256);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = static_cast<uint8_t>(i);
    }

    // Calculate checksum all at once
    uint32_t crc_full = 0xFFFFFFFFu;
    crc_full = crc32cCompute(data.data(), data.size(), crc_full);
    crc_full ^= 0xFFFFFFFFu;

    // Calculate checksum incrementally (two parts)
    uint32_t crc_incremental = 0xFFFFFFFFu;
    crc_incremental = crc32cCompute(data.data(), 100, crc_incremental);
    crc_incremental = crc32cCompute(data.data() + 100, data.size() - 100, crc_incremental);
    crc_incremental ^= 0xFFFFFFFFu;

    EXPECT_EQ(crc_full, crc_incremental) << "Incremental calculation should match full calculation";
}

// Edge case: Minimum page size with maximum data
TEST(CRC32C_Comprehensive, MinimumPageWithMaxData)
{
    constexpr uint32_t PAGE_SIZE = 8192;
    std::vector<uint8_t> page(PAGE_SIZE);

    // Fill entire page (except header) with 0xFF
    std::fill(page.begin() + sizeof(PageHeader), page.end(), 0xFF);

    initializeCanonicalChecksummedPage(page, PAGE_TYPE_HEAP, 0);
    EXPECT_TRUE(validatePageChecksum(page.data(), PAGE_SIZE))
        << "Page filled with 0xFF should validate correctly";
}

// Edge case: Maximum page size
TEST(CRC32C_Comprehensive, MaximumPageSize)
{
    constexpr uint32_t PAGE_SIZE = 131072; // 128KB
    std::vector<uint8_t> page(PAGE_SIZE, 0);

    // Fill with pattern
    for (size_t i = sizeof(PageHeader); i < PAGE_SIZE; i++) {
        page[i] = static_cast<uint8_t>((i * 7) % 256);
    }

    initializeCanonicalChecksummedPage(page, PAGE_TYPE_HEAP, 0);
    EXPECT_TRUE(validatePageChecksum(page.data(), PAGE_SIZE))
        << "Maximum size page should validate correctly";
}
