/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_runtime.h"

namespace scratchbird::sblr::jit
{
    auto JitRuntime::onDependencySignatureChange(const core::ID& object_uuid,
                                                 core::ErrorContext* ctx)
        -> core::Status
    {
        return artifact_store_.retireByObjectOnDependencySignatureChange(object_uuid, ctx);
    }

    auto JitRuntime::onSecurityPolicyVersionChange(const core::ID& object_uuid,
                                                   core::ErrorContext* ctx)
        -> core::Status
    {
        return artifact_store_.retireByObjectOnPolicyVersionChange(object_uuid, ctx);
    }
}

