# Decision Record

## Current section decision
Section `17` is anchored to code-backed stored routine and language-UDR behavior, not to the older broad checklist model.

## Decisions locked by this audit pass
1. `CatalogManager` plus `Executor` are the primary authority for functions and procedures.
2. Stored routine persistence is catalog-backed and TOAST-backed; it is not a detached external package model.
3. Language UDR support in current code is primarily a registration, selection, capability-hash, sandbox, and compile-boundary subsystem.
4. Builtin emulation package manifests are real and authoritative for the currently audited engine package inventory.
5. Remote connector and cluster-fabric materials in section `17` must stay fail-closed around the audited catalog and connector surfaces.
6. Blob-filter support is catalog-backed in current code, but runtime invocation is not proven here and must not be documented as an implemented execution pipeline.

## Explicit non-decisions
- no claim of a complete generic external function runtime ABI
- no claim of a complete operator-facing remote connector execution guarantee matrix
- no claim of live cluster-fabric task orchestration in this section without further proof
- no claim of runtime blob-filter invocation without code-backed closure
