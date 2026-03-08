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

#include "scratchbird/sblr/jit/jit_artifact_store.h"

namespace scratchbird::sblr::jit
{
    auto verifyLlvmArtifactEnvelope(const std::vector<uint8_t>& native_blob,
                                    const ArtifactCompatibilityKey& key,
                                    std::string& detail_out) -> bool;
}
