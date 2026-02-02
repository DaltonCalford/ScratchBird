/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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
