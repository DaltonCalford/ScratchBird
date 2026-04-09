# Index Management SQL (Native Parser)

## Purpose
Define the native SQL surface for index maintenance, relocation, diagnostics, and reporting. These statements map to SBLR operations; the engine does not parse SQL.

## Scope
These statements are supported only by the native parser. Emulated parsers may expose subsets or different syntax.

## Maintenance Statements
### ALTER INDEX ... SET
```
ALTER INDEX <index_name>
  SET (option = value, ...);
```
- Updates per-index options in `index_option`.
- Options are validated against index type constraints.

### ALTER INDEX ... RESET
```
ALTER INDEX <index_name>
  RESET (option, ...);
```
- Removes per-index overrides. Missing values fall back to index type defaults and then global defaults.

### ALTER INDEX ... REBUILD
```
ALTER INDEX <index_name>
  REBUILD [ONLINE|OFFLINE]
  [WITH (target_fillfactor = <int>, throttle_ms = <int>)];
```
- Creates a shadow index and swaps on completion.
- Maps to `IDX_REBUILD` SBLR.

### ALTER INDEX ... REBALANCE
```
ALTER INDEX <index_name>
  REBALANCE [ONLINE|OFFLINE]
  [WITH (target_fillfactor = <int>, throttle_ms = <int>)];
```
- Rebuilds pages to reduce fragmentation without changing filespace.
- Maps to `IDX_REBALANCE` SBLR.

### ALTER INDEX ... RELOCATE
```
ALTER INDEX <index_name>
  RELOCATE TO FILESPACE <filespace_name>
  [ONLINE|OFFLINE]
  [WITH (max_bytes_per_txn = <int>, throttle_ms = <int>)];
```
- Copies index pages to target filespace and swaps root/meta on completion.
- Maps to `IDX_RELOCATE` SBLR.

### ANALYZE INDEX
```
ANALYZE INDEX <index_name>
  [WITH (sample_rate = <float>)];
```
- Updates `index_stats` for planner cost estimates.
- Maps to `IDX_ANALYZE` SBLR.

### LIGHT SCAN
```
ALTER INDEX <index_name>
  LIGHT SCAN
  [WITH (sample_pages = <int>, throttle_ms = <int>)];
```
- Samples page headers and pointer structure to estimate health.
- Maps to `IDX_LIGHT_SCAN` SBLR.

### DIAGNOSTIC SCAN (VALIDATE)
```
ALTER INDEX <index_name>
  DIAGNOSTIC SCAN
  [WITH (throttle_ms = <int>)];
```
- Full structural verification of all pages and entries.
- `VALIDATE INDEX <index_name>` is an alias.
- Maps to `IDX_DIAGNOSTIC_SCAN` SBLR.

## Default Option Management
### ALTER INDEX DEFAULTS
```
ALTER INDEX DEFAULTS FOR <index_type>
  SET (option = value, ...);
```
- Stores per-index-type defaults in catalog table `index_type_default_option`.
- Defaults are applied during `CREATE INDEX` when the option is not specified.

### ALTER INDEX DEFAULTS RESET
```
ALTER INDEX DEFAULTS FOR <index_type>
  RESET (option, ...);
```
- Removes per-index-type overrides and falls back to global defaults.

## Reporting Statements
### SHOW INDEX HEALTH
```
SHOW INDEX HEALTH <index_name>;
```
- Returns `index_health` row for the index.

### SHOW INDEX USAGE
```
SHOW INDEX USAGE <index_name>;
```
- Returns `index_usage` row.

### SHOW INDEX STORAGE
```
SHOW INDEX STORAGE <index_name>;
```
- Returns `index_storage` row.

### SHOW INDEX CONTENTION
```
SHOW INDEX CONTENTION <index_name>;
```
- Returns `index_contention` row.

### SHOW INDEX OPTIONS
```
SHOW INDEX OPTIONS <index_name>;
```
- Returns effective options (per-index overrides merged with per-type and global defaults).

## SBLR Mapping
Native parser statements must map to the following canonical SBLR operations:
- `IDX_SET_OPTIONS`
- `IDX_RESET_OPTIONS`
- `IDX_REBUILD`
- `IDX_REBALANCE`
- `IDX_RELOCATE`
- `IDX_ANALYZE`
- `IDX_LIGHT_SCAN`
- `IDX_DIAGNOSTIC_SCAN`
- `IDX_SHOW_HEALTH`
- `IDX_SHOW_USAGE`
- `IDX_SHOW_STORAGE`
- `IDX_SHOW_CONTENTION`
- `IDX_SHOW_OPTIONS`

## Test Contract
- ALTER INDEX SET/RESET updates catalog options.
- REBUILD/REBALANCE/RELOCATE operate online and offline with defined throttling.
- LIGHT SCAN reports warning/error for detected corruption patterns.
- DIAGNOSTIC SCAN reports corrupt when structural invariants fail.
- SHOW commands return consistent rows from catalog and monitoring tables.
