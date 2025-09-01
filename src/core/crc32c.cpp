#include <cstddef>
#include <cstdint>
#include <array>

namespace scratchbird {
namespace core {

// Software CRC32C (Castagnoli) table-driven implementation
static constexpr std::array<uint32_t, 256> kCrc32cTable = []{
	std::array<uint32_t, 256> table{};
	// Reflected polynomial for CRC32C (Castagnoli)
	const uint32_t poly = 0x82F63B78u;
	for (uint32_t i = 0; i < 256; ++i) {
		uint32_t crc = i;
		for (int j = 0; j < 8; ++j) {
			crc = (crc & 1u) ? ((crc >> 1) ^ poly) : (crc >> 1);
		}
		table[i] = crc;
	}
	return table;
}();

uint32_t crc32c_compute(const uint8_t* data, size_t length, uint32_t initial) {
	uint32_t crc = initial;
	for (size_t i = 0; i < length; ++i) {
		crc = kCrc32cTable[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
	}
	return crc;
}

} // namespace core
} // namespace scratchbird

