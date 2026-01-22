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
