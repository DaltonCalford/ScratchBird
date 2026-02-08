#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_schema.h"
#include "scratchbird/sblr/v3_types.h"

namespace scratchbird::sblr::v3 {

void canonicalizeSymbols(std::vector<std::string>& symbols, std::vector<size_t>* remap = nullptr);
void canonicalizeConstants(std::vector<ConstantPoolEntry>& pool, std::vector<size_t>* remap = nullptr);

void canonicalizePayload(const SchemaDef& schema, Value& payload);

}  // namespace scratchbird::sblr::v3
