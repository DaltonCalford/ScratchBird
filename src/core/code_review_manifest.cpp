/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file code_review_manifest.cpp
 * @brief Build-compiled architecture manifest for source-only reviews.
 *
 * This translation unit exists to make the current runtime topology and
 * maturity boundaries explicit in the compiled source tree. It intentionally
 * contains no executable logic.
 */

namespace scratchbird::core::review_manifest
{
    auto runtimeTopology() -> const char*
    {
        return "scratchbird_core + scratchbird_sblr are the canonical engine "
               "runtime; sb_listener_* + sb_parser_* form the deployed "
               "multi-protocol front-door; the engine IPC path executes "
               "SBLR/precompiled payloads rather than parsing arbitrary client "
               "SQL text in-engine.";
    }

    auto layeringNotes() -> const char*
    {
        return "scratchbird_pool is an experimental auxiliary pool/cache layer "
               "used for cache primitives and test-side scaffolding. It is "
               "distinct from scratchbird::core::ConnectionPool and is not an "
               "installed product runtime surface; scratchbird_udr contains "
               "language/tool modules, dynamic "
               "SQL->SBLR helper endpoints, and outbound remote-engine or "
               "cluster bridge connectors separate from authoritative "
               "engine-auth enforcement in scratchbird_security.";
    }

    auto maturityNotes() -> const char*
    {
        return "The LLVM JIT lane now includes real toolchain detection, "
               "provider-linked LLVM artifact emission, persisted payload "
               "verification, queue dedupe, fallback tiering, and artifact-"
               "backed runtime selection. Direct native callable lowering "
               "remains a later implementation step.";
    }

    auto emulationNotes() -> const char*
    {
        return "Native ScratchBird retains real tablespace/filespace support. "
               "Emulated engines expose sandboxed compatibility behavior and "
               "catalog views without granting direct client file access.";
    }
}
