# JIT Runtime Policy Queue and Hotness Model

Status: current_authority

## Purpose

Define the exact policy merge, queue admission, hotness threshold, and performance-counter model used by the current JIT runtime.

## Core Runtime Defaults

Current runtime defaults include:
- queue capacity `128`
- hotness threshold `3`
- artifact signature requirement disabled by default
- compile backend present through `JitCompiler`

Policy defaults in the runtime envelope are:
- compile mode: `EXPLICIT_ONLY`
- execution policy: `INTERPRETED_ONLY`

## Effective Policy Resolution

Current code resolves effective policy by combining:
- database policy
- session policy
- object policy

### Compile mode merge

Compile mode is merged by the most restrictive rule:
- if any layer is `EXPLICIT_ONLY`, the effective compile mode is `EXPLICIT_ONLY`
- otherwise the effective compile mode is `JIT_ALLOWED`

### Execution policy merge

Execution policy is merged by this precedence:
1. `INTERPRETED_ONLY`
2. `REQUIRE_NATIVE`
3. `PREFER_NATIVE`

In other words:
- if any layer is `INTERPRETED_ONLY`, native execution is not permitted
- else if any layer is `REQUIRE_NATIVE`, native execution is required
- else the effective policy is `PREFER_NATIVE`

### Hint propagation

Current runtime carries the hint envelope through unchanged:
- `disable_compile`
- `disable_execute`
- `prefer_vm`

## Queue Admission Model

The current queue is a bounded FIFO queue with dedupe.

Each queue entry carries:
- queue id
- object UUID
- module id
- plan id
- compile request
- dedupe key
- priority byte

## Current Queue Semantics

### Enqueue

`tryEnqueue` must:
1. reject a duplicate non-empty `dedupe_key` with `COMPILE_ALREADY_QUEUED`
2. reject a full queue with `QUEUE_SATURATED`
3. append accepted entries at the tail

### Dequeue

`tryDequeue` must:
1. remove from the head
2. clear the dedupe key from the pending-key set
3. preserve FIFO order

### Capacity change

`setCapacity` must:
1. update queue capacity
2. drop tail entries until depth fits the new capacity
3. clear dedupe keys for dropped entries

## Important Current Boundary

The queue entry carries a `priority` field, but current queue behavior is still FIFO.

That means:
- priority is preserved in the queue contract
- priority does not currently reorder queue admission or dequeue
- any future priority scheduler must be promoted explicitly, not inferred from the presence of the field

## Hotness and Queue Interaction

Current runtime tracks hotness separately from queue depth.

The runtime performance model already records:
- queue enqueued count
- queue duplicate count
- queue saturated count
- queue current depth
- queue max depth
- hotness below threshold count
- hotness promotion count

This means the JIT runtime must keep:
- queue saturation distinct from hotness non-eligibility
- explicit compile requests distinct from hotness-driven compile promotion

## Required Fail-Closed Rules

1. Duplicate compile requests must not silently widen queue depth.
2. Saturated queue state must return an explicit reason.
3. Policy merge must not silently weaken a more restrictive database, session, or object policy.
4. Priority must not be treated as active scheduling authority until a real priority scheduler exists.

## Cross-Section References

- `23_SBLR_VM_Compiler_and_Executor/NATIVE_COMPILATION_AND_ARTIFACT_LIFECYCLE.md`
- `41_Platform_Interface_and_Lifecycle_Management/LLVM_AND_ACCELERATOR_TOOLCHAIN_RUNTIME_MODEL.md`
