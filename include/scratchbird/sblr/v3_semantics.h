#pragma once

#include <cstdint>

namespace scratchbird::sblr::v3 {

struct OpcodeSemantics {
    int stack_in = 0;
    int stack_out = 0;
    bool is_expression = false;
    bool is_statement = false;
    bool requires_lock_order = false;
};

OpcodeSemantics getOpcodeSemantics(uint16_t opcode);

}  // namespace scratchbird::sblr::v3
