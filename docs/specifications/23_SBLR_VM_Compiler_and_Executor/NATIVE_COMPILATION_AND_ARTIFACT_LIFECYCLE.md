# Native Compilation and Artifact Lifecycle

Status: current_authority

## Purpose

This file defines the current native-compilation surface for SBLR-executable objects, the policy envelope, compatibility key, compile queue behavior, persisted artifact model, verification, fallback, and invalidation behavior.

## Current code-backed authority

Current implementation provides:
- native-eligible routine surface gating
- policy envelope resolution across database, session, and object scope
- hotness-gated and explicit compile paths
- compile queue dedupe and saturation handling
- deterministic IR lowering
- backend selection between LLVM and null fallback
- persisted artifact storage and verification
- runtime artifact selection and fallback
- artifact retirement on dependency-signature or security-policy drift
- performance counters for dispatch, queueing, compile latency, fallback, and per-object behavior

## Current eligible surfaces

Native eligibility is currently limited to:
- `FUNCTION`
- `TRIGGER`
- `PROCEDURE`
- `PACKAGE_MEMBER`

All other surfaces are current non-authority for native execution and must remain on VM or interpreted paths.

## Policy envelope

Current policy resolution merges three layers:
- database policy
- session policy
- object policy

### Compile mode

Current compile-mode vocabulary:
- `EXPLICIT_ONLY`
- `JIT_ALLOWED`

Current resolution rule:
- if any layer is `EXPLICIT_ONLY`, the effective mode is `EXPLICIT_ONLY`
- otherwise the effective mode is `JIT_ALLOWED`

### Execution policy

Current execution-policy vocabulary:
- `INTERPRETED_ONLY`
- `PREFER_NATIVE`
- `REQUIRE_NATIVE`

Current resolution rule:
- `INTERPRETED_ONLY` dominates all weaker choices
- otherwise `REQUIRE_NATIVE` dominates `PREFER_NATIVE`
- otherwise the effective policy is `PREFER_NATIVE`

### Hints

Current per-request hints are:
- `disable_compile`
- `disable_execute`
- `prefer_vm`

Hints may only narrow native use, not widen it beyond the effective policy envelope.

## Compatibility key

Every compile request and persisted artifact is keyed by:
- object UUID
- canonical SBLR hash
- target triple
- CPU feature profile
- native ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

## Queue dedupe key

The compile queue dedupe key is the compatibility key projected into a stable string over:
- object UUID
- canonical SBLR hash
- target triple
- CPU feature profile
- native ABI version
- compiler identity
- compiler version
- optimization profile
- security policy version

## Queue model

Current queue behavior is:
- bounded capacity queue, default `128`
- FIFO dequeue order
- duplicate suppression by dedupe key
- queue saturation refusal

The `priority` field exists in the current queue entry structure, but current code does not use it for ordering. It is therefore non-authoritative for scheduling semantics today.

## Hotness model

Current hotness behavior is:
- threshold-based promotion
- default threshold `3`
- threshold key based on object UUID, target triple, and native ABI version
- threshold `0` normalizes to `1`

If a routine has not reached the threshold, the runtime records `HOTNESS_BELOW_THRESHOLD` and remains on the VM path.

## Lowering model

Current lowering is deliberately conservative.

Current code-backed lowering:
- verifies opcode legality first
- preserves the canonical SBLR byte stream
- emits an isomorphic lowered IR payload
- marks `preserves_side_effect_order = true`

That means current native compilation is not yet a semantic rewrite pipeline. It is a deterministic artifact-generation lane over a legality-checked canonical routine.

## Opcode legality gate

Current legality rejection occurs when:
- canonical routine is empty
- SBLR container decode fails
- container module name declares unsupported JIT opcode family
- bytecode begins with the reserved unsupported marker

## Artifact creation pipeline

Current explicit compile pipeline is:
1. verify native-eligible surface kind
2. require non-empty canonical SBLR
3. require non-zero object, module, and plan ids
4. require a populated compatibility key
5. lower canonical SBLR to legality-checked IR
6. require preserved side-effect order
7. invoke backend compile
8. on success, materialize a `READY` artifact and persist it through the artifact store

## Runtime selection algorithm

Current `selectPath` behavior is:
1. reject non-native-eligible surfaces to VM
2. resolve effective policy
3. honor `INTERPRETED_ONLY`, `disable_execute`, and `prefer_vm` before artifact lookup
4. fetch a verified artifact for the exact compatibility key
5. if a valid artifact exists, choose `NATIVE`
6. if verification fails, record fallback and retire unusable artifacts for load or payload failure classes
7. if policy is `REQUIRE_NATIVE` and no valid artifact exists, return `ERROR`
8. otherwise remain on VM and, when allowed, queue recompilation
9. if no artifact exists and compile mode is `JIT_ALLOWED`, queue compilation subject to hotness

## Queue drain algorithm

Current queue drain behavior is:
1. dequeue FIFO until empty
2. compile each queued request
3. materialize and persist successful artifacts
4. record queued compile success or failure counts
5. update current queue depth and compile-latency metrics

## Invalidation and retirement

Current artifact retirement triggers are:
- dependency-signature change
- security-policy version change
- artifact verification failure classes that imply unusable payloads

Current retirement action is deletion of:
- artifact stats row
- artifact catalog row

## Current fallback model

Current fallback rules are:
- null backend forces deterministic VM fallback
- backend unavailability does not redefine correctness
- load or payload failure increments fallback counters
- interpreted or VM execution remains authoritative when native execution is unavailable or untrusted

## Current observability surfaces

Current runtime counters include at least:
- VM dispatch count
- native dispatch count
- error dispatch count
- compile queue enqueue, duplicate, saturation, current depth, and max depth
- hotness below threshold and hotness promotion counts
- explicit and queued compile success or failure counts
- total and last compile latency
- native execution count and CPU time
- fallback count
- load-failure count
- retired-unusable-artifact count
- per-object dispatch, queue, compile, hotness, and fallback counters

## Current bounded gap

Current code proves artifact generation, persistence, verification, dispatch selection, and fallback.
It does not yet prove a universally closed direct callable-native execution pipeline for every legal routine family. Native artifact selection is current authority; universal native execution closure remains a later step.
