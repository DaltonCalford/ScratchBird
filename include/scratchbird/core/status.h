#pragma once

#include <cstdint>

namespace scratchbird {
namespace core {

enum class Status : uint32_t {
	Ok = 0,
	FileNotFound = 1001,
	FileExists = 1002,
	IoError = 1003,
	PageCorrupt = 2001,
	ChecksumMismatch = 2002,
	Deadlock = 3001,
	LockTimeout = 3002,
};

} // namespace core
} // namespace scratchbird

