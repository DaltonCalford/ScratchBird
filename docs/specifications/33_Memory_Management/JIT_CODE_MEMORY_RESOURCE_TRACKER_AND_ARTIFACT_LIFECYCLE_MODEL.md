Status: current_authority_beta1

# JIT Code Memory Resource Tracker and Artifact Lifecycle Model

## Purpose

This document defines the Beta 1 memory model for ScratchBird native-code
generation. It separates:

- compile scratch
- object-link or relocation memory
- executable code pages
- persistent artifact payloads
- published runtime handles

The goal is to make JIT memory reclaimable, observable, and compatible with
Section 23 artifact selection rules.

## Hard invariants

1. JIT code pages are not anonymous process heap.
2. Compile scratch shall never own published code.
3. Published code shall be reclaimable by resource tracker.
4. The VM path remains the deterministic fallback.
5. Section 23 compatibility and signature admission still controls selection.
   This file controls memory ownership and lifetime only.

## Canonical memory partitions

The JIT runtime shall expose these memory partitions:

| Partition | Domain | Lifetime |
| --- | --- | --- |
| compile scratch arena | `jit_metadata_domain` | one compile job |
| object-link arena | `jit_metadata_domain` | one materialization job |
| artifact metadata heap | `jit_metadata_domain` | artifact row lifetime |
| executable code heap | `jit_code_domain` | published code lifetime |
| loaded artifact handle table | `jit_code_domain` | loaded artifact lifetime |

## Resource tracker hierarchy

Every published compiled unit shall bind to:

1. `PROCESS_ROOT`
2. `jit_code_domain`
3. `DATABASE_ROOT`
4. `SCHEMA_ROOT`
5. `RESOURCE_TRACKER_ROOT`

The tracker key is:

- object UUID
- canonical SBLR hash
- target triple
- CPU profile
- ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

## Publish state machine

Required states:

1. `QUEUED`
2. `COMPILING`
3. `LINKING`
4. `READY_UNPUBLISHED`
5. `PUBLISHED`
6. `RETIRED`
7. `RECLAIMABLE`

Transitions:

1. `QUEUED` to `COMPILING` after queue lease
2. `COMPILING` to `LINKING` after code generation succeeds
3. `LINKING` to `READY_UNPUBLISHED` after relocation and verification succeed
4. `READY_UNPUBLISHED` to `PUBLISHED` only after code pages are charged and
   tracker metadata is installed
5. `PUBLISHED` to `RETIRED` on incompatibility, policy drift, or explicit retire
6. `RETIRED` to `RECLAIMABLE` after no execution handle remains

## Admission rules

1. Compile scratch reserves bytes in `jit_metadata_domain`.
2. Code publication reserves bytes in `jit_code_domain`.
3. If compile scratch is denied, compilation is not started.
4. If code publication is denied:
   - the compiled payload is not published
   - the VM path continues unless `REQUIRE_NATIVE` forbids fallback
5. Retired artifacts remain metadata-visible until the tracker confirms
   reclaimability.

## Default limits

| Tunable | Default |
| --- | --- |
| `sb.jit.compile_jobs_max` | `4` |
| `sb.jit.compile_scratch_max_mb` | `512` |
| `sb.jit.code_heap_max_mb` | `1024` |
| `sb.jit.loaded_handle_max` | `4096` |
| `sb.jit.retired_reclaim_batch` | `64` |

## Publish flow

```cpp
PublishResult publishCompiledUnit(const CompiledUnit& unit, JitTracker& tracker) {
  auto codeLease = reserveBytes(tracker.codeNode(), unit.code_bytes, MemoryClass::PublishedCode);
  if (!codeLease.ok()) {
    return PublishResult::fallback("CODE_HEAP_DENIED");
  }
  auto codeRange = codeHeap.allocateExecutable(unit.code_bytes, unit.alignment);
  if (!codeRange.ok()) {
    codeLease.release();
    return PublishResult::fallback("CODE_ALLOC_FAILED");
  }
  installCode(unit, codeRange.value());
  tracker.markPublished(codeRange.value(), unit.code_bytes);
  return PublishResult::native(codeRange.value());
}
```

## Retirement flow

```cpp
void retireTracker(JitTracker& tracker, RetirementReason why) {
  tracker.state = TrackerState::RETIRED;
  tracker.reason = why;
  if (tracker.active_calls == 0) {
    codeHeap.release(tracker.code_range);
    tracker.state = TrackerState::RECLAIMABLE;
  }
}
```

## Required observability

The engine shall expose:

- compile scratch bytes
- code heap committed bytes
- artifact metadata bytes
- published unit count
- retired unit count
- reclaim latency
- publication denials
- fallback reason codes

## Cross-section references

- `../23_SBLR_VM_Compiler_and_Executor/NATIVE_COMPILATION_AND_ARTIFACT_LIFECYCLE.md`
- `../23_SBLR_VM_Compiler_and_Executor/LLVM_JIT_PROVIDER_ARTIFACT_SELECTION_AND_VM_FALLBACK_MODEL.md`
- `MEMORY_BUDGET_TREE_BREAKER_AND_SCHEMA_QUOTA_MODEL.md`
- `MEMORY_PRESSURE_BACKPRESSURE_AND_ADMISSION.md`
