# Memory Pressure Backpressure and Admission

Status: current_authority_beta1

## Purpose

Define the Beta 1 memory-pressure protocol, backpressure behavior, emergency
reserve handling, and admission ladder for ScratchBird.

## Pressure and admission matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| cache-local pressure reaction | beta1_required | caches must trim against their own budgets before escalating | not a free bypass around parent limits |
| request admission under pressure | beta1_required | admission flows through the breaker tree and the staged pressure ladder | not a cluster-wide quota system |
| backpressure signals | beta1_required | pressure actions must publish explicit reason codes and state transitions | not an undocumented heuristic bus |
| graceful degradation under pressure | beta1_required | degradation occurs through shrink, spill, wait, and cancel in fixed order | not process-kill tolerance |

## Canonical staged pressure ladder

The owning node shall attempt these stages in order:

1. `LOCAL_TRIM`
   - trim derivative caches within the same domain
2. `OPTIONAL_DROP`
   - pause warm-load
   - drop optional accelerator sidecars
   - retire cold result and translation entries
3. `SPILL_OR_DEFER`
   - switch eligible operators to spill
   - queue memory debt for non-urgent cleanup
4. `WAIT_FOR_RECLAIM`
   - wait up to the configured reclaim timeout
5. `CANCEL_WORK`
   - cancel non-spillable statement or compile work
6. `EMERGENCY_ONLY`
   - reserve only for emergency-eligible operations

## Canonical admission order

For a memory-hungry action, the owning subsystem must apply this order:

1. validate that the request belongs to the correct memory domain
2. refuse oversize single-entry admission if the subsystem cannot legally split it
3. attempt domain-local trim or invalidation only inside the same domain
4. fall back to spill or workfile path if that operator family explicitly supports spill
5. downgrade optional accelerator sidecars before degrading canonical resident state
6. wait for reclaim only inside the configured timeout window
7. refuse or cancel the action if safe execution cannot be preserved

## Mandatory cross-domain rules

1. Permission cache pressure must not be resolved by silently discarding
   transaction or visibility state.
2. Translation cache pressure must not be resolved by cross-parser sharing
   shortcuts.
3. Statement cache pressure must not be resolved by widening unsafe plan reuse
   across schema or privilege boundaries.
4. Resident vector pressure must not be charged to executor scratch or
   temp-spill domains without explicit accounting.
5. Spill admission does not authorize any subsystem to bypass MGA durability or
   visibility rules.

## Reclaim wait defaults

| Tunable | Default |
| --- | --- |
| `sb.mem.reclaim_wait_ms` | `25` |
| `sb.mem.statement_cancel_after_wait` | `true` |
| `sb.mem.pause_jit_on_soft_pressure` | `true` |
| `sb.mem.pause_warm_load_on_soft_pressure` | `true` |

## Sample pressure handler

```cpp
bool runPressureProtocol(MemoryNode& node, MemoryNode& leaf, uint64_t bytes, MemoryClass cls) {
  if (trimLocalCaches(node, bytes)) {
    return true;
  }
  if (dropOptionalState(node, bytes)) {
    return true;
  }
  if (cls.spillable() && enableSpill(leaf, bytes)) {
    return true;
  }
  if (waitForReclaim(node, std::chrono::milliseconds(25))) {
    return true;
  }
  return cancelOrRefuse(leaf, cls);
}
```

## Required refusal behavior

1. Non-spillable statements shall fail before entering unbounded growth.
2. Compile jobs denied in `jit_metadata_domain` or `jit_code_domain` shall
   fall back to VM unless `REQUIRE_NATIVE` forbids fallback.
3. Resident-index warm load may be refused. Canonical already-published state
   may not be silently discarded to admit new warm work.
4. Memory-pressure incidents shall emit a pressure event with node path, domain,
   reason code, and bytes requested.

## Explicit non-guarantees

- no claim that every operator family already spills automatically
- no claim that process kill by the operating system is an acceptable normal path
