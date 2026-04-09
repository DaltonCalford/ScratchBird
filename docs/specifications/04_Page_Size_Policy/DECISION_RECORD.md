# Section 04 Decision Record

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Decisions now grounded in code
- Page size is fixed for a database instance at creation time.
- The currently supported sizes are `8192`, `16384`, `32768`, `65536`, and `131072` bytes.
- Database creation fails closed on unsupported page sizes.
- Database open fails closed if the persisted header page size is invalid.
- Database header persistence stores page size both in `page_header.page_size` and in `block_size`.
- Tablespaces must match the database page size; variable page size per filespace remains rejected in the implementation.
- Large-page behavior is structural:
  - page lower/special helper math uses a larger unit when `page_size > 65535`
  - heap item pointers use a 32-bit offset with a 31-bit length field

## Implementation code map
- `ScratchBird/include/scratchbird/core/ondisk.h:1003`
- `ScratchBird/include/scratchbird/core/ondisk.h:1014`
- `ScratchBird/include/scratchbird/core/ondisk.h:1020`
- `ScratchBird/include/scratchbird/core/ondisk.h:1034`
- `ScratchBird/include/scratchbird/core/ondisk.h:1044`
- `ScratchBird/include/scratchbird/core/ondisk.h:1355`
- `ScratchBird/src/core/database.cpp:4281`
- `ScratchBird/src/core/database.cpp:4328`
- `ScratchBird/src/core/database.cpp:4338`
- `ScratchBird/src/core/database.cpp:4403`
- `ScratchBird/src/core/database.cpp:4666`
- `ScratchBird/src/core/page_manager.cpp:1665`
- `ScratchBird/src/core/backup_manager.cpp:562`
- `ScratchBird/src/core/backup_manager.cpp:605`
- `ScratchBird/include/scratchbird/core/heap_page.h:37`
- `ScratchBird/src/core/heap_page.cpp:381`

## Drift and contradictions
- The old decision record promoted `16KB` as if it were a canonical engine default. The reviewed code does not prove that default authority.
- The old record was too vague about large-page support. The reviewed code proves precise header/unit and heap-structure effects instead.
- The old record did not capture restore-time and tablespace-open page-size validation strongly enough.

## Non-blocking expansion candidates
- A true canonical operator-default page-size policy, if one is desired
- A per-subsystem compatibility register for all supported page sizes
- Dedicated measured tradeoff evidence to justify recommendation language for `8KB`/`16KB`/`32KB`/`64KB`/`128KB`

## Suggestions
- Keep the fixed-per-database invariant as the section’s primary authority.
- Treat default/tuning language as recommendation only until the code owns it.
