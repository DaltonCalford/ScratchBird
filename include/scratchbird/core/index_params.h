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

#include "scratchbird/core/gpid.h"
#include <cstdint>
#include <string>

namespace scratchbird::core
{

struct BloomIndexParams
{
    bool enabled = false;
    double target_fpr = 0.01;
    uint8_t bits_per_key = 0;
    uint8_t num_hashes = 0;
    GPID meta_gpid = 0;
};

struct IndexParams
{
    bool has_bloom = false;
    BloomIndexParams bloom{};
};

std::string serializeIndexParams(const IndexParams &params);
bool parseIndexParams(const std::string &params_str, IndexParams *params_out);

} // namespace scratchbird::core
