# Dependencies - 06_Fixed_Bootstrap_Page_Map

## Upstream dependencies
- `05_Page_Taxonomy_and_Binary_Layouts`
- `02_Filespace_Lifecycle` for separation between main-file bootstrap pages and tablespace-local bootstrap pages

## Runtime owners and dependents
- `Database` owns bootstrap creation and open-time validation
- `CatalogManager` owns durable payload population and validation of the fixed catalog root at page `2`
- `PageManager` consumes the fixed FSM root at page `3`
- `TransactionManager` consumes and extends transaction-map pages rooted at page `4`
- `SweepManager`, `GarbageCollector`, and checkpoint or startup-reconciliation code consume system-state control metadata from page `1`
- `BackupManager` and restore validation depend on the page-zero and bootstrap-map contract remaining stable

## Test and gate dependents
- bootstrap constant and layout tests
- page-management edge-case tests
- database format compatibility tests
- restore validation rehearsal tests
- startup, sweep, and system-state durability tests

## Critical boundary dependency
This section depends on keeping two bootstrap surfaces distinct:
- main database bootstrap map: fixed pages `0..5`
- per-tablespace bootstrap map: local page `0..1` inside each tablespace file

The previous prose blurred these two surfaces. The implementation does not.
