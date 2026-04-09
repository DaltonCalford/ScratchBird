# Table Storage and Access Method Architecture

Status: current_authority

## Purpose

Define the current umbrella contract for table storage modes, row movement, overflow and version placement, and access-method boundaries.

## Current authority

Current source proves:
- StorageEngine is the public mutation and scan surface for heap tuple insert, delete, update, visibility, and scan behavior
- heap tuple layout, stable row identity, version chains, and TOAST flags live in HeapPage and TupleHeader contracts
- back-version placement and same-extent locality heuristics are real in current StorageEngine code
- online migration routing and dual-source tablespace resolution are real in TIDResolver
- B-tree and columnstore families are page-rooted access methods
- LSM is a file-backed access-method family with its own persistence model

## Current model

ScratchBird does not currently prove one unified table-access-method abstraction or one centralized storage-mode authority module. The current authoritative model is explicit mixed-family ownership with shared heap and relocation semantics where the code proves them.

## Required rules

1. heap tuple and version-chain semantics remain authoritative for heap-backed table storage
2. stable row identity and row movement legality remain explicit rather than inferred
3. back-version placement and relocation decisions must remain fail closed when locality or legality checks fail
4. access-method family boundaries must remain explicit; page-rooted and file-backed families must not be silently unified in the spec
5. online migration routing must not be overclaimed as universal relocation support for every family or case

## Family boundary summary

- heap storage family: StorageEngine plus HeapPage own tuple mutation, versioning, and TOAST interaction
- page-rooted access methods: B-tree and columnstore own their rooted page contracts
- file-backed access methods: LSM owns its directory and compaction-based persistence model

## Non-guarantees

- no claim is made here that online relocation exists for every case
- no claim is made here that shrink or rewrite automation is fully present
- no claim is made here that one unified table-access-method abstraction currently governs all families
- no claim is made here that relation-level storage modes are centralized in one authoritative runtime registry
