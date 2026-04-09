# Index Build and Maintenance

## Added Requirements
- online maintenance publish may not precede durable metadata publication
- post-cutover cleanup must respect section-10 publication ordering
- savepoint rollback markers in maintenance-delta logs must prevent resurrection
  of rolled-back entries during later apply
- checkpoint interruption must leave either the pre-cutover or post-cutover
  generation queryable, never an ambiguous middle state

## Update 2026-03-28: current code-backed build boundary

Current directly re-proven build and maintenance authority is narrower than the requirements above.

Directly proven in this pass:
- catalog create paths allocate index root pages, write index records, instantiate the runtime through `IndexFactory`, and cache the live object
- expression and partial index create paths persist payloads through TOAST-backed storage before runtime instantiation
- index metadata already carries shadow or lifecycle-oriented fields such as:
  - `logical_index_id`
  - `state`
  - `valid_from_xid`
  - `retired_xid`
  - build start and completion timestamps

Current bounded or unproven boundary:
- the current pass does not close one generic operator-facing online or concurrent build contract
- the current pass does not close one generic rebalance, relocate, light-scan, or diagnostic-scan implementation surface across all families
- the current pass does not re-prove the broad maintenance-state transition set as a unified runtime contract

Highest-value next proof areas:
- B-tree rebuild and cutover
- family-specific health-scan implementations
- shadow-index or maintenance delta application rules

## Update 2026-03-28: maintenance proof states

Current maintenance proof states in section `18` are:
- `proven_now`:
  - create-time publication through catalog plus `IndexFactory`
  - lifecycle-oriented metadata fields carried in `IndexInfo`
- `partial`:
  - shadow or rebuild-oriented lifecycle intent
- `unsupported_by_audit`:
  - universal online build
  - universal concurrent build
  - universal rebalance
  - universal relocate
  - universal health-scan contract

Future maintenance hardening must preserve this split instead of silently promoting target-state behavior to current truth.
