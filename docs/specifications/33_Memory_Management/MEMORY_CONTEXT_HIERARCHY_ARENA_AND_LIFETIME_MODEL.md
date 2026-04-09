Status: current_authority_beta1

# Memory Context Hierarchy Arena and Lifetime Model

## Purpose

This document defines the Beta 1 in-process memory-context hierarchy and typed
arena model used by ScratchBird. It turns the high-level Section 33 ownership
story into an implementation contract.

## Hard invariants

1. All substantial allocations shall belong to an engine-owned memory context.
2. Context lifetime and allocator type are separate decisions, but both must be
   explicit.
3. Longer-lived contexts may not depend on shorter-lived storage for
   correctness.
4. Context teardown must be deterministic on statement failure, transaction
   rollover, and worker death.

## Canonical context hierarchy

The engine shall organize memory into the following ownership layers:

1. `PROCESS_CONTEXT`
2. `DOMAIN_CONTEXT`
3. `DATABASE_CONTEXT`
4. `SCHEMA_ROOT_CONTEXT`
5. `CONNECTION_CONTEXT`
6. `TRANSACTION_CONTEXT`
7. `STATEMENT_CONTEXT`
8. `OPERATOR_CONTEXT`
9. `RESULT_CONTEXT`
10. `SCRATCH_CONTEXT`
11. `RESOURCE_TRACKER_CONTEXT`

`RESOURCE_TRACKER_CONTEXT` is used for long-lived objects that are neither
connection-bound nor statement-bound, such as published JIT units and spill-page
groups.

## Default allocator binding by context

| Context kind | Default allocator | Lifetime |
| --- | --- | --- |
| `PROCESS_CONTEXT` | `SbObjectHeap` | process |
| `DOMAIN_CONTEXT` | `SbGenerationArena` | process |
| `DATABASE_CONTEXT` | `SbGenerationArena` | attachment |
| `SCHEMA_ROOT_CONTEXT` | `SbGenerationArena` | database or schema-root policy lifetime |
| `CONNECTION_CONTEXT` | `SbArena` | connection |
| `TRANSACTION_CONTEXT` | `SbArena` | one transaction |
| `STATEMENT_CONTEXT` | `SbArena` | one statement |
| `OPERATOR_CONTEXT` | `SbPageBackedArena` if spillable else `SbArena` | one operator |
| `RESULT_CONTEXT` | `SbArena` or `SbPageBackedArena` by result size | one result lifetime |
| `SCRATCH_CONTEXT` | `SbArena` | one bounded phase |
| `RESOURCE_TRACKER_CONTEXT` | `SbGenerationArena`, `SbCodeHeap`, or `SbSlab` | tracker lifetime |

## Canonical allocator classes

The runtime shall implement these allocator classes:

1. `SbArena`
   - append-only or chunked arena
   - bulk reset
   - default for parse, bind, plan, and short executor state
2. `SbGenerationArena`
   - generation-tagged arena
   - reclaim by generation or epoch
   - default for metadata, tracker state, and medium-lived domain state
3. `SbSlab`
   - fixed-size object allocator
   - default for queue nodes, descriptors, stats rows, and tracker entries
4. `SbPageBackedArena`
   - allocates page groups that can remain resident or spill
   - required for spillable operators and large transient row groups
5. `SbCodeHeap`
   - executable page allocator
   - used only for published native code
6. `SbObjectHeap`
   - general fallback heap
   - allowed only where the typed allocators are a poor fit

## Ownership rules

1. Each allocation has exactly one owning context.
2. Shared access is permitted. Shared ownership is not.
3. Ownership transfer to a longer-lived context must be explicit.
4. Ownership transfer to a shorter-lived context is forbidden.
5. Context-local allocators may free memory only through the owning context.

## Lifetime ordering

Destruction order shall proceed from shorter-lived to longer-lived:

1. `SCRATCH_CONTEXT`
2. `RESULT_CONTEXT`
3. `OPERATOR_CONTEXT`
4. `STATEMENT_CONTEXT`
5. `TRANSACTION_CONTEXT`
6. `CONNECTION_CONTEXT`
7. `SCHEMA_ROOT_CONTEXT` children
8. `DATABASE_CONTEXT`
9. `DOMAIN_CONTEXT`
10. `PROCESS_CONTEXT`

## Transaction rule

ScratchBird is always in a transaction. When a transaction ends, the old
transaction context shall be retired before the successor transaction context is
installed.

## Required flows

### Connection start

1. create `CONNECTION_CONTEXT` under the appropriate schema-root node
2. create long-lived connection-local slabs and arenas
3. create the first `TRANSACTION_CONTEXT`

### Transaction rollover

1. fence new transaction-local allocations
2. finish rollback or commit cleanup for the retiring transaction
3. destroy the retiring `TRANSACTION_CONTEXT`
4. create the successor `TRANSACTION_CONTEXT`

### Statement execution

1. create `STATEMENT_CONTEXT`
2. allocate parse, bind, and plan state from statement arenas
3. create one `OPERATOR_CONTEXT` per physical operator family instance
4. create bounded `SCRATCH_CONTEXT` children for phase-local work
5. destroy scratch, result, and operator contexts before destroying the
   statement context

### Background worker start

1. create `TASK_ROOT` in the budget tree
2. bind a matching `RESOURCE_TRACKER_CONTEXT`
3. allocate all worker state beneath it
4. release or fence debt rows before teardown

## Arena-selection rules

Statement and operator execution shall prefer arena-style allocation for:

- AST nodes and normalized SQL state
- binder and rewrite structures
- expression evaluation scratch
- tuple projection scratch
- temporary sort or hash metadata
- short-lived result rendering buffers

`SbSlab` is preferred for:

- descriptor structs
- queue nodes
- telemetry rows
- lease or tracker records

`SbPageBackedArena` is required for:

- spillable hash tables
- spillable sort runs
- vector or ANN temporary row groups larger than arena thresholds

`SbCodeHeap` is required for:

- executable JIT code pages

## Threshold rules

Beta 1 defaults:

| Threshold | Default |
| --- | --- |
| `sb.mem.arena_chunk_kb` | `64` |
| `sb.mem.page_backed_min_kb` | `256` |
| `sb.mem.slab_large_object_bytes` | `512` |
| `sb.mem.result_page_backed_min_kb` | `1024` |

Allocations above `page_backed_min_kb` must prefer `SbPageBackedArena` when the
owning path is spillable.

## Sample statement flow

```cpp
StatementContext openStatementContext(ConnectionContext& conn, const StatementId& stmtId) {
  auto& txn = conn.currentTransaction();
  auto stmtNode = memoryTree.createStatementNode(txn.memoryNode(), stmtId);
  StatementContext stmt{
      .memory_node = stmtNode,
      .arena = SbArena::create(stmtNode, "statement-arena"),
      .descriptor_slab = SbSlab::create(stmtNode, sizeof(RuntimeDescriptor), "statement-descriptors"),
  };
  return stmt;
}
```

## Failure rules

1. Statement failure shall reclaim statement, operator, result, and scratch
   contexts before control returns to the client.
2. Transaction rollback shall reclaim the retiring transaction context before a
   new transaction begins accumulating state.
3. Worker death shall leak no owned executable code or page-backed temporary
   pages after lease recovery completes.

## Non-conforming behavior

The following are non-conforming:

1. leaking operator-owned objects into process-global caches
2. storing transaction-owned pointers in process-global structures
3. binding published JIT code to statement scratch lifetime
4. storing resident-index state in statement or transaction scope
5. using `SbObjectHeap` for every allocation because it is convenient
