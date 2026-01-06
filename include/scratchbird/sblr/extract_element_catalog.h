#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include "scratchbird/sblr/opcodes.h"

namespace scratchbird::sblr
{
    struct ElementArgSpec
    {
        uint8_t min_args = 0;
        uint8_t max_args = 0;
    };

    std::optional<ExtractField> resolveExtractFieldName(std::string_view name);
    const char* extractFieldToString(ExtractField field);
    ElementArgSpec extractFieldArgSpec(ExtractField field);
} // namespace scratchbird::sblr
