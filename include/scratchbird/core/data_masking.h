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
#include <string>
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{
    enum class MaskingType : uint8_t
    {
        NONE = 0,     // No masking
        PARTIAL = 1,  // Show subset based on pattern
        FULL = 2      // Hide all data
    };

    struct MaskingConfig
    {
        MaskingType type = MaskingType::NONE;
        std::string pattern;              // e.g., "XXX-XX-####" for SSN
        std::string full_mask_char = "*"; // Character to use for FULL masking
    };

    class DataMasking
    {
    public:
        static Status applyMasking(const std::string& value,
                                   const MaskingConfig& config,
                                   bool has_privilege,
                                   std::string& masked_out,
                                   ErrorContext* ctx = nullptr);

        static Status parsePattern(const std::string& pattern,
                                   std::vector<char>& parsed_out,
                                   ErrorContext* ctx = nullptr);

    private:
        static Status applyPartialMasking(const std::string& value,
                                         const std::string& pattern,
                                         const std::string& mask_char,
                                         std::string& masked_out,
                                         ErrorContext* ctx);

        static Status applyFullMasking(const std::string& value,
                                       const std::string& mask_char,
                                       std::string& masked_out,
                                       ErrorContext* ctx);
    };
} // namespace scratchbird::core
