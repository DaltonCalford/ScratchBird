# Platform Interface and Filesystem Assumptions

Status: current_authority

## Objective

Define the operating-system and filesystem assumptions that are currently required for maintained ScratchBird certification lanes.

## Current authoritative assumptions

The current certification model assumes:
- stable local filesystem semantics for page writes, directory creation, rename, delete, and metadata persistence
- process, socket, and file-descriptor behavior sufficient for the current listener, parser, and engine topology
- local monotonic and wall-clock services sufficient for timeout, telemetry, and ordering surfaces already documented in canonical specs
- basic crash recovery conditions consistent with current MGA checkpoint and startup recovery semantics

## Required filesystem properties

A platform in current certification scope must provide:
1. readable and writable regular files for database, control, and artifact paths
2. directory creation and removal with deterministic failure signaling
3. exclusive-lock or equivalent coordination semantics used by current runtime surfaces
4. durable flush or sync behavior consistent with the current durability model
5. predictable path, permission, and socket semantics for the maintained listener and tooling stack

## Non-authoritative assumptions

This section does not claim:
- universal certification for distributed filesystems
- universal certification for network filesystems with weak durability or rename semantics
- equivalence across every container, hypervisor, or remote volume environment
- support for filesystems that cannot satisfy current dirty-page, checkpoint, control-socket, or artifact-retention requirements

## Fail-closed rule

If a platform or filesystem cannot satisfy the properties above, it is outside current certification scope and must be treated as unsupported until a maintained certification lane is added to section 31.
