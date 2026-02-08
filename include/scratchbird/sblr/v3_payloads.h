#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "scratchbird/sblr/v3_schema.h"
#include "scratchbird/sblr/v3_types.h"

namespace scratchbird::sblr::v3 {

const SchemaDef* schemaForOpcode(uint16_t opcode);

bool encodeInstructionWithSchema(const Instruction& inst, Buffer& out, DecodeError& err);
bool decodeInstructionWithSchema(const uint8_t* data, size_t size, size_t& offset, Instruction& out, DecodeError& err);

}  // namespace scratchbird::sblr::v3
