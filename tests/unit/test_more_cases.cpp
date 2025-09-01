#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>

#include "scratchbird/core/ondisk.h"

using namespace scratchbird::core;

TEST(MoreCases, BadChecksumOnSystemCatalog) {
	uint32_t ps = 8192;
	std::vector<uint8_t> page(ps, 0);
	auto* h = reinterpret_cast<PageHeader*>(page.data());
	h->magic = kMagicSBRD;
	h->version = 1;
	h->page_type = PAGE_TYPE_SYSTEM_CATALOG;
	h->page_size = ps;
	h->checksum = calculate_page_checksum(page.data(), ps);
	// Tamper to invalidate checksum
	page[ps - 32] ^= 0xAA;
	EXPECT_FALSE(validate_page_checksum(page.data(), ps));
}

