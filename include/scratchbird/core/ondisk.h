#pragma once

#include <cstdint>
#include <cstddef>

namespace scratchbird {
namespace core {

// Page types per ON_DISK_FORMAT.md
enum PageType : uint16_t {
	PAGE_TYPE_DATABASE_HEADER = 0,
	PAGE_TYPE_SYSTEM_CATALOG  = 1,
	PAGE_TYPE_FREE_SPACE_MAP  = 2,
	PAGE_TYPE_HEAP            = 3,
	PAGE_TYPE_BTREE_META      = 4,
	PAGE_TYPE_BTREE_INTERNAL  = 5,
	PAGE_TYPE_BTREE_LEAF      = 6,
	PAGE_TYPE_TRANSACTION_MAP = 7,
	PAGE_TYPE_CATALOG_ROOT    = 8,  // Root page for system catalog
};

// Fixed 64-byte page header; little-endian integers assumed
#pragma pack(push, 1)
struct PageHeader {
	uint32_t magic;        // 0x00 'SBRD'
	uint16_t version;      // 0x04 format version
	uint16_t page_type;    // 0x06 PageType
	uint32_t page_size;    // 0x08 8192|16384|32768|65536|131072
	uint32_t checksum;     // 0x0C CRC32C of [0x10..page_size)

	uint64_t lsn;          // 0x10
	uint32_t page_id;      // 0x18
	uint32_t flags;        // 0x1C

	uint8_t  database_uuid[16]; // 0x20 UUID v7 bytes

	uint64_t generation;   // 0x30
	uint16_t free_space;   // 0x38
	uint16_t item_count;   // 0x3A
	uint16_t free_offset;  // 0x3C
	uint16_t special_size; // 0x3E
};
#pragma pack(pop)

constexpr uint32_t kMagicSBRD = 0x53425244; // 'SBRD' little-endian

// CRC32C API (implemented in core)
uint32_t crc32c_compute(const uint8_t* data, size_t length, uint32_t initial);

inline uint32_t calculate_page_checksum(const uint8_t* page, uint32_t page_size) {
	// initial value 0xFFFFFFFF, process [0x00..0x0B] and [0x10..page_size)
	uint32_t crc = 0xFFFFFFFFu;
	crc = crc32c_compute(page, 12, crc);
	crc = crc32c_compute(page + 16, page_size - 16, crc);
	return crc ^ 0xFFFFFFFFu;
}

inline bool validate_page_checksum(const uint8_t* page, uint32_t page_size) {
	auto header = reinterpret_cast<const PageHeader*>(page);
	return header->checksum == calculate_page_checksum(page, page_size);
}

inline bool is_valid_alpha_page_size(uint32_t page_size) {
	return page_size == 8192u || page_size == 16384u || page_size == 32768u ||
	       page_size == 65536u || page_size == 131072u;
}

} // namespace core
} // namespace scratchbird

