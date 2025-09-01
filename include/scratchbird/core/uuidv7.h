#pragma once

#include <cstdint>
#include <array>

namespace scratchbird {
namespace core {

struct UuidV7Bytes {
	std::array<uint8_t, 16> bytes{};
};

// Generate RFC 9562 UUID v7 bytes (time-ordered). Implementation in core.
UuidV7Bytes generate_uuid_v7();

} // namespace core
} // namespace scratchbird

