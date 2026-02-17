#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "scratchbird/sblr/v3_types.h"

namespace scratchbird::sblr::v3 {

struct ValidationResult {
    bool ok = false;
    std::string code;
    std::string message;
    std::size_t instruction_offset = 0;
    uint16_t opcode = 0;
    std::string canonical_opcode_symbol;
};

ValidationResult validateContainerDetailed(const uint8_t* data, size_t size);
bool validateContainer(const uint8_t* data, size_t size, std::string& err);

// Section-04 vNext IR contract validators.
ValidationResult validateVNextOpcodeContract(const Instruction& inst);
ValidationResult validateVNextEncodedInstructionContract(const uint8_t* data, size_t size);
ValidationResult validateVNextRewriteEvidenceContract(const Value::Object& evidence);

}  // namespace scratchbird::sblr::v3
