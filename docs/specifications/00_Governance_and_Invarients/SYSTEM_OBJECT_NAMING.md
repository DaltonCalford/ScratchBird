# System Object Naming

Status: current_authority

## Current naming contract

- durable internal identity is UUIDv7-backed
- shared ID aliases are reused across multiple subsystems rather than redefined locally
- storage and resolver layers follow the same durable ID contract
- user-facing labels and names are secondary to durable identity

## Primary authority anchors

- ScratchBird/include/scratchbird/core/types.h
- ScratchBird/include/scratchbird/core/uuidv7.h
- ScratchBird/include/scratchbird/core/storage_engine.h
- ScratchBird/include/scratchbird/core/tid_resolver.h

## Required rules

1. shared durable ID remains UUIDv7-backed
2. local subsystem aliases must not drift away from the shared type
3. label changes must not redefine durable identity
4. reserved-name or label governance must not override durable identity governance

## Non-guarantees

- no claim is made here that every system label or reserved namespace has been re-audited in this pass
- no claim is made here that one central naming registry already exists
- no claim is made here that all downstream sections already distinguish labels from durable IDs cleanly
