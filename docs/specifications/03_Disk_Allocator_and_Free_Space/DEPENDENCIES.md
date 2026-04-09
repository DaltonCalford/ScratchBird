# Section 03 Dependencies

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current implementation dependencies
- `PageManager` depends on shared on-disk page contracts, database file I/O, tablespace header decode, and bootstrap FSM state.
- `BufferPool` depends on `Database` page read or write APIs, page-header legality, checksum validation, GPID identity, and commit or checkpoint durability fences.
- `GarbageCollector` depends on transaction reclaim horizons, heap-page validity, version-chain audit, buffer-pool pin/unpin APIs, and publication into index cleanup.
- `Database` owns canonical buffer-policy config loading, checkpoint-bound drain orchestration, write-admission fencing, bootstrap FSM page creation, and writeback incident persistence.
- Section `03` feeds section `02` filespace lifecycle, section `05` page layout and integrity, section `10` sweep or reclaim ordering, and section `31` reliability or checkpoint gates.

## Implementation code map
- `ScratchBird/include/scratchbird/core/page_manager.h:32,42,101,113,146,168,188,208,233,249,290,297,309,328,390`
- `ScratchBird/src/core/page_manager.cpp:105,957,1321,1598,1788,1916,2188,2308`
- `ScratchBird/include/scratchbird/core/buffer_pool.h:56,248,392,482,494,531,558,579,596,599,600`
- `ScratchBird/src/core/buffer_pool.cpp:3262,3270,3359,3453,3517,3661,4894,4999,5039,5285`
- `ScratchBird/include/scratchbird/core/garbage_collector.h:117,132,135,140,153,160,165,174,186,251`
- `ScratchBird/src/core/garbage_collector.cpp:362,395,432,447,461,486,569,573,586,657,708,757,832,860,973,983`
- `ScratchBird/include/scratchbird/core/database.h:522,524,525,541`
- `ScratchBird/src/core/database.cpp:1405,1420,1541,1731,1745,1759,1775,1783,1884,1905,1920,1928,2693,2699,2780,3005,3024,4039,4484,5680,5692`

## Drift and contradictions
- Older dependency prose treated more of section `03` as a single allocator subsystem than the reviewed code supports.
- The current dependency truth is deliberately split across allocation, buffer policy, GC, and database checkpoint state.
- Some future-facing surfaces such as NUMA and advanced workload classification do not currently have matching implementation dependencies.

## Non-blocking expansion candidates
- A machine-readable dependency matrix separating runtime dependencies from test and gate dependencies
- A single operator evidence surface for allocation pressure, dirty debt, and sweep-blocked cleanup state

## Suggestions
- Keep dependency ownership explicit in future tickets so section `03` changes do not drift back into one oversized pseudo-subsystem.
