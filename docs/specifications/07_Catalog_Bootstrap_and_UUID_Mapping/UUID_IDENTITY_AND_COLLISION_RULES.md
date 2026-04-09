# UUID Identity and Collision Rules

## Status
`current_authority_with_reconstructed_expansion`

## Last code-audit date
`2026-03-27`

## Purpose
Define the engine's authoritative UUID-based internal identity rules and narrow the specification to the collision behavior the current implementation actually proves.

## Current implementation status
The current implementation clearly uses UUIDv7 as the durable internal identity format:
- `core::ID` is an alias of `UuidV7Bytes`
- database creation generates and persists a database UUID into the on-disk header
- database open reloads that UUID and generates a separate runtime `server_instance_id`
- heap insert and versioning paths generate or preserve stable `row_uuid` identity across back-version creation and rewrite flows
- catalog code generates UUIDv7 identifiers broadly for many durable object classes

## Implementation code map
- `ScratchBird/include/scratchbird/core/uuidv7.h:23`: `UuidV7Bytes`
- `ScratchBird/include/scratchbird/core/uuidv7.h:60`: `generateUuidV7`
- `ScratchBird/include/scratchbird/core/uuidv7.h:94`: `using ID = UuidV7Bytes`
- `ScratchBird/src/core/uuidv7.cpp:19`: UUIDv7 generator implementation entry point
- `ScratchBird/include/scratchbird/core/ondisk.h:335`: database header stores durable `database_uuid`
- `ScratchBird/include/scratchbird/core/ondisk.h:1064`: helper writes UUID into header
- `ScratchBird/include/scratchbird/core/ondisk.h:1070`: helper reloads UUID from header
- `ScratchBird/src/core/database.cpp:4297`: database UUID generated at create time
- `ScratchBird/src/core/database.cpp:4298`: database header persists generated UUID
- `ScratchBird/src/core/database.cpp:4464`: open path reloads database UUID from the header
- `ScratchBird/src/core/database.cpp:4725`: runtime holds the durable database UUID after open
- `ScratchBird/src/core/database.cpp:4727`: server instance id is generated separately at runtime
- `ScratchBird/src/core/heap_page.cpp:79`: insert path checks whether `row_uuid` is absent
- `ScratchBird/src/core/heap_page.cpp:87`: insert path generates `row_uuid` when missing
- `ScratchBird/src/core/heap_page.cpp:1006`: back-version path preserves stable row UUID
- `ScratchBird/src/core/heap_page.cpp:1205`: rewrite path carries the preferred stable row UUID forward
- `ScratchBird/src/core/heap_page.cpp:1274`: primary tuple path falls back to UUID generation only when a stable value is absent
- `ScratchBird/src/core/catalog_manager.cpp:53264`: representative catalog object id generation using `generateUuidV7()`

## Known contradictions and drift
- this audit proves UUID generation, persistence, and stable propagation; it does not prove one centralized cross-workgroup or cross-cluster collision-governance subsystem
- the previous prose was too aggressive about global collision coordination and should be narrowed to fail-closed local durability and identity stability rules
- uniqueness enforcement may still be delegated to local table or catalog constraints in some domains rather than one engine-wide UUID incident surface

## Non-blocking expansion candidates
- publish a machine-readable inventory of every durable UUID-bearing structure and the subsystem that owns it
- decide whether cross-workgroup UUID collision governance is a real future requirement or an explicit non-goal
- add a dedicated operator or diagnostic surface for UUID identity incident reporting if the project wants collision observability beyond local constraint failures
- add explicit section-level gate coverage for database UUID reopen invariants and stable row UUID preservation across all versioning paths
