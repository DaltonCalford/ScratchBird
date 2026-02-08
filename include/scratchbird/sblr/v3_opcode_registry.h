#pragma once

#include <cstdint>
#include <string_view>

namespace scratchbird::sblr::v3 {

#include "scratchbird/sblr/v3_opcodes.generated.h"

const char* opcodeName(uint16_t opcode);
bool isKnownOpcode(uint16_t opcode);

}  // namespace scratchbird::sblr::v3
