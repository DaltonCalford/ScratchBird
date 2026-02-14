#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace scratchbird::sblr::v3 {

// Maps transport opcode names to canonical section-22 symbolic IDs.
std::string canonicalOpcodeSymbolForV3Name(std::string_view v3_name);
std::string canonicalOpcodeSymbolForOpcode(uint16_t opcode);

// True when the mapped canonical symbol exists in authoritative feature matrix.
bool opcodeMapsToCanonicalFeature(uint16_t opcode);
bool opcodeMapsToCanonicalFeatureName(std::string_view v3_name);

}  // namespace scratchbird::sblr::v3

