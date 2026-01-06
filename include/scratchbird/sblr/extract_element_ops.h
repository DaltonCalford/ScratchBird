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
