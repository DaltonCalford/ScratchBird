#include <gtest/gtest.h>
#include <cstdint>
#include <array>
#include <vector>
#include <cstring>
#include <random>

#include "scratchbird/version.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

static void fill_page(std::vector<uint8_t>& page, uint32_t page_size, uint32_t page_id, uint16_t page_type) {
	ASSERT_GE(page_size, 64u);
	page.assign(page_size, 0);
	auto* header = reinterpret_cast<PageHeader*>(page.data());
	header->magic = kMagicSBRD;
	header->version = 1;
	header->page_type = page_type;
	header->page_size = page_size;
	header->lsn = 0;
	header->page_id = page_id;
	header->flags = 0;
	// UUID v7 bytes
	auto uuid = generate_uuid_v7();
	std::memcpy(header->database_uuid, uuid.bytes.data(), uuid.bytes.size());
	header->generation = 1;
	header->free_space = 0;
	header->item_count = 0;
	header->free_offset = sizeof(PageHeader);
	header->special_size = 0;
	// Compute checksum
	header->checksum = calculate_page_checksum(page.data(), page_size);
}

TEST(Version, VersionStringPrefix) {
	std::string vs = SCRATCHBIRD_VERSION_STRING;
	ASSERT_NE(vs.find("ScratchBird v"), std::string::npos);
}

TEST(OnDiskFormat, AlphaPageSizes) {
	EXPECT_TRUE(is_valid_alpha_page_size(8192));
	EXPECT_TRUE(is_valid_alpha_page_size(16384));
	EXPECT_TRUE(is_valid_alpha_page_size(32768));
	EXPECT_FALSE(is_valid_alpha_page_size(65536));
	EXPECT_FALSE(is_valid_alpha_page_size(131072));
}

TEST(OnDiskFormat, HeaderLayoutAndChecksum) {
	for (uint32_t size : {8192u, 16384u, 32768u}) {
		std::vector<uint8_t> page;
		fill_page(page, size, 0, PAGE_TYPE_DATABASE_HEADER);
		auto* header = reinterpret_cast<const PageHeader*>(page.data());
		// Magic and basic fields
		EXPECT_EQ(header->magic, kMagicSBRD);
		EXPECT_EQ(header->version, 1);
		EXPECT_EQ(header->page_type, PAGE_TYPE_DATABASE_HEADER);
		EXPECT_EQ(header->page_size, size);
		EXPECT_EQ(header->page_id, 0u);
		// Checksum must validate
		EXPECT_TRUE(validate_page_checksum(page.data(), size));
		// Tamper then expect mismatch
		page[64] ^= 0xFF;
		EXPECT_FALSE(validate_page_checksum(page.data(), size));
	}
}

TEST(CRC32C, KnownVectors) {
	// Empty input
	uint32_t crc = crc32c_compute(nullptr, 0, 0xFFFFFFFFu) ^ 0xFFFFFFFFu;
	EXPECT_EQ(crc, 0x00000000u);
	// "123456789" standard CRC32C = 0xE3069283
	const uint8_t msg[] = {'1','2','3','4','5','6','7','8','9'};
	uint32_t c = 0xFFFFFFFFu;
	c = crc32c_compute(msg, sizeof(msg), c);
	c ^= 0xFFFFFFFFu;
	EXPECT_EQ(c, 0xE3069283u);
}

TEST(UUIDv7, StructureAndMonotonicity) {
	const int N = 1000;
	std::array<uint8_t, 16> prev{};
	for (int i = 0; i < N; ++i) {
		auto u = generate_uuid_v7();
		// Version (bits 48..51) == 0b0111 in byte[6] high nibble
		EXPECT_EQ((u.bytes[6] >> 4) & 0x0F, 0x07);
		// Variant 0b10xxxxxx in byte[8] bits 6..7
		EXPECT_EQ((u.bytes[8] >> 6) & 0x03, 0x02);
		if (i > 0) {
			// Compare timestamp portion bytes[0..5] lexicographically non-decreasing
			for (int b = 0; b < 6; ++b) {
				if (u.bytes[b] != prev[b]) {
					EXPECT_GE(u.bytes[b], prev[b]);
					break;
				}
			}
		}
		prev = u.bytes;
	}
}