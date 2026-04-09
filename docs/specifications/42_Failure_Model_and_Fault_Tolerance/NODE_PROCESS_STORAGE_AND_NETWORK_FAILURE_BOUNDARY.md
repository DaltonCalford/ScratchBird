# Node Process Storage and Network Failure Boundary

This file owns the boundary between local MGA-state failures and broader system
or network assumptions.

## Fault-boundary matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| local process failure | current_authority | restart reconciles durable inventory, page state, prepared state, and metadata state | not transparent process failover |
| storage corruption or partial write | current_authority | header, checksum, generation, repair-state, and transaction-inventory validation classify storage failure | not full online self-healing |
| local protocol or IPC failure | current_bounded | listener, parser, and IPC surfaces may fail closed or retry in bounded local ways | not multi-node recovery |
| local evidence-lane persistence failure | current_authority | shadow-capture persistence may block prune; `wal_after` persistence may backlog without changing truth | not core recovery authority |
| remote archive transport failure | current_authority | remote-database archive delivery may fail independently of database correctness | not a correctness dependency |
| node or host failure | fail_closed | node-wide fault tolerance remains fail-closed unless explicitly proven elsewhere | not HA cluster behavior |
| network partition | fail_closed | no partition-tolerant continuity guarantee exists | not consensus or quorum behavior |

## Canonical rules

1. local failures and multi-node failures must be separated explicitly
2. storage-failure language defers to MGA durability, page legality, and
   containment truth
3. local evidence-lane failures do not become transaction truth
4. remote network delivery failures for derivative lanes do not invalidate local
   committed MGA truth
5. host-wide or network-wide resilience remains fail closed unless directly
   proven

## Explicit non-guarantees

- no transparent host failover
- no cluster partition recovery guarantee
- no consensus or quorum guarantee
- no complete online repair system
