#pragma once

#include <cstdint>

namespace scratchbird {
namespace core {

enum class Status : uint32_t {
	Ok = 0,
	FileNotFound = 1001,
	FileExists = 1002,
	IoError = 1003,
	InvalidPath = 1004,
	PermissionDenied = 1005,
	InvalidArgument = 1006,
	PageCorrupt = 2001,
	ChecksumMismatch = 2002,
	Deadlock = 3001,
	LockTimeout = 3002,
	OOM = 3003,  // Out of memory per ERROR_HANDLING.md
	PageFull = 4001,  // No space available in page
	NotFound = 4002,  // Tuple/item not found
};

} // namespace core
} // namespace scratchbird

