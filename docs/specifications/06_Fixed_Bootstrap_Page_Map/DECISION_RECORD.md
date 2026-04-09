# Decision Record - 06_Fixed_Bootstrap_Page_Map

## Scope
- fixed bootstrap pages for the main database file
- bootstrap page ids and page roles
- open-time bootstrap-map validation
- boundaries between bootstrap placement and subsystem-owned payloads

## Invariants
- the main database file bootstrap map is fixed at pages `0..5`
- startup must not scan for these roots dynamically
- page `1` is a durable system-state control surface
- page `2` is the canonical fixed catalog root location
- page `3` is the canonical main-file FSM root location
- page `4` is the canonical TIP root location
- the fixed bootstrap map is distinct from per-tablespace local page `0..1` contracts

## Decisions confirmed by code audit
- fixed main-file bootstrap constants in `ondisk.h` are authoritative
- `Database::create` writes the bootstrap pages directly and stamps the database header with canonical root references
- `Database::open` validates the bootstrap map before runtime subsystem initialization continues
- the catalog root payload is owned by catalog code after fixed placement, not by a static create-time entry table written in `Database::create`
- the transaction-map root is a concrete TIP page with bootstrap entries, not only a root pointer shell
- only one reserved bootstrap page is proven in the current implementation

## Alternatives rejected by current implementation
- dynamic bootstrap root discovery
- treating page `1` as only a human-facing settings page
- treating tablespace-local bootstrap pages as part of the same contract as the main database bootstrap map
- assuming an implemented reserved extension band beyond page `5`

## Remaining open questions
- whether the section should eventually define a stricter canonical payload contract for page `2`, or continue delegating that payload shape to catalog-owned contracts while keeping section `06` focused on placement and validation
- whether page `5` will stay inert or receive a dedicated owning subsystem in a later format revision
