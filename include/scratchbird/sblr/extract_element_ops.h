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

#include <string>
#include <vector>
#include "scratchbird/core/typed_value.h"
#include "scratchbird/sblr/opcodes.h"

// Reference: docs/specifications/EXTRACT_AND_ALTER_ELEMENT.md
namespace scratchbird::sblr
{
    bool extractElement(const core::TypedValue& source,
                        ExtractField field,
                        const std::vector<core::TypedValue>& args,
                        core::TypedValue* out,
                        std::string* error);

    bool alterElement(const core::TypedValue& source,
                      ExtractField field,
                      const std::vector<core::TypedValue>& args,
                      const core::TypedValue& new_value,
                      core::TypedValue* out,
                      std::string* error);
} // namespace scratchbird::sblr
