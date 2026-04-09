# Section 04 Specification Outline

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
This section is now narrowed to five code-backed claims:
- supported fixed page sizes are validated by `isValidAlphaPageSize`
- page size is persisted in the database header and mirrored in `block_size`
- database open revalidates page size from the stored header before allocating the full header buffer
- secondary tablespaces must use the same page size as the database
- large-page structural consequences are specific helper and layout rules, not a broad runtime “mode”

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/ondisk.h` | `isValidAlphaPageSize` | `1355` | Canonical supported page-size validator |
| ScratchBird | `include/scratchbird/core/ondisk.h` | `pageLower`, `pageUpper`, `pageSpecial` | `1003`, `1009`, `1014` | Header offset helpers switch unit scaling for page sizes above `65535` |
| ScratchBird | `include/scratchbird/core/ondisk.h` | `pageSetLower`, `pageSetUpper`, `pageSetSpecial` | `1020`, `1034`, `1044` | Header offset and special-space writers for all supported page sizes |
| ScratchBird | `src/core/database.cpp` | `Database::init_header_page` | `4281` | Persists `page_header.page_size` and `block_size` in the database header |
| ScratchBird | `src/core/database.cpp` | `Database::create` page-size validation | `4403` | Rejects unsupported sizes with explicit accepted values |
| ScratchBird | `src/core/database.cpp` | `Database::open` header page-size validation | `4666` | Rejects invalid persisted page sizes before full header load |
| ScratchBird | `src/core/page_manager.cpp` | Tablespace `page_size` mismatch rejection | `1665` | Enforces one page size across database and tablespaces |
| ScratchBird | `src/core/backup_manager.cpp` | Restore-time page-size validation and `block_size` match | `562`, `605` | Restore validation rechecks persisted page size and block-size equality |
| ScratchBird | `include/scratchbird/core/heap_page.h` | `ItemPointer` | `37` | 32-bit offset and 31-bit length layout |
| ScratchBird | `src/core/heap_page.cpp` | Max tuple-size bound derived from page size | `381` | Heap tuple capacity scales from page size minus fixed structures |

## Current implementation depth
- Supported sizes are explicit and fail-closed.
- Per-database fixed page size is real.
- Cross-tablespace mixed page sizes are rejected.
- Large-page structural support is real for header offsets and heap item-pointer layout.
- The current code does not prove a separate operator config plane for choosing page size beyond database creation/open and validation paths.

## Non-blocking expansion questions
- Should section `04` keep any canonical `storage.page_size` wording if the reviewed engine authority is currently the creation API rather than a proven config control?
- Should `16KB` remain only a recommendation, or should a real engine default-selection surface be implemented and documented?
- Do all storage/index families have explicit tested coverage across `64KB` and `128KB`, or is the current guarantee mostly driven by shared page/header invariants plus targeted tests?

## Non-blocking expansion candidates
- A page-size capability matrix across access methods, indexes, and maintenance subsystems
- A proven canonical configuration or admin surface for page-size selection if that is intended policy
- A single operator guidance document for page-size choice tradeoffs grounded in measured behavior rather than inherited prose

## Suggestions
- Keep this section narrowly tied to structural and validation truth that is directly proven in code.
- Move performance tuning advice into recommendation language unless backed by dedicated performance evidence.
