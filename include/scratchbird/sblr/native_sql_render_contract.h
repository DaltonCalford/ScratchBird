#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "scratchbird/sblr/v3_codec.h"

namespace scratchbird::sblr {

enum class NativeSqlResultShape : uint8_t {
    COMMAND_STATUS = 0,
    ROWSET_OR_MUTATION = 1,
    STREAM_STATUS = 2,
};

struct NativeSqlRenderContract {
    const char* contract_id;
    uint16_t opcode;
    const char* canonical_opcode_symbol;
    const char* grammar_signature;
    NativeSqlResultShape result_shape;
    const char* classifier_key_prefix;
};

const NativeSqlRenderContract* nativeSqlRenderContractForInstruction(
    const v3::Instruction& instruction);

const NativeSqlRenderContract* nativeSqlRenderContractForOpcode(uint16_t opcode);

const NativeSqlRenderContract* nativeSqlRenderContractTable(size_t& count);

const char* nativeSqlResultShapeName(NativeSqlResultShape shape);

}  // namespace scratchbird::sblr

