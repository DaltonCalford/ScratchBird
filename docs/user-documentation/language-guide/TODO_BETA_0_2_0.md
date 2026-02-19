# Future TODO (0.2.0)
Last modified: 2026-02-19

Back links:
- [Language Guide README](README.md)
- [Beta 0.2.0 Workplan](../../planning/BETA_0_2_0_WORKPLAN_2026-02-19.md)
- [Beta 0.2.0 Backlog](../../planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md)

## Required Before 0.2.0

1. Cube runtime semantic bridge closure
- Current `0.1.0` state: parser+emitter support exists for `CREATE/ALTER/DROP CUBE`, `REFRESH CUBE`, `SHOW CUBE STATS`, but executor routes cube opcodes through deterministic bridge rejection (`BRG_0406`).
- Required closure:
  - Dedicated executor semantic handlers for cube DDL variants.
  - Dedicated refresh and stats runtime paths.
  - Integration tests changed from expected rejection to expected success.

2. Admin and NoSQL semantic bridge closure
- Current `0.1.0` state: admin/cluster/NoSQL opcode families are parsed/emitted, but remain bridge-partial in runtime.
- Required closure:
  - Replace bridge stubs with explicit semantic handlers by command family.
  - Add end-to-end positive execution tests for each family.

3. Lifecycle gap closure for object families still partial
- Complete full `CREATE+ALTER+SHOW/DESCRIBE+DROP` closure for remaining partial object families documented in this guide.

4. Master parser parity closure (all emulated-engine functionality surfaced in v3)
- Current baseline: NSQL registry has `178` rows, all mandatory-scope, with `141` currently unmapped/open.
- Required closure:
  - `mandatory_scope_rows == total_rows` (all rows in v3 scope).
  - `unmapped_rows == 0`.
  - no feature remains engine-only without a v3 surface contract.

5. Full language-reference refresh for all changed v3 surface
- Scope: full tree under `docs/user-documentation/language-guide/`.
- Required closure:
  - every new/modified command/object has updated lifecycle documentation.
  - operators/functions/casts/domains/index/context coverage reflects implementation.
  - README and navigation links are complete and remain within project tree.

## Detailed Execution Artifacts

- Master execution plan: `docs/planning/BETA_0_2_0_WORKPLAN_2026-02-19.md`
- Itemized backlog: `docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md`
- Master parser parity gate baseline: `docs/planning/native_sql/gates/NSQL-GATE-05/V3_MASTER_PARSER_PARITY_BASELINE_2026-02-19.md`
- Gate-05 promoted coverage summaries:
  - `docs/planning/native_sql/gates/NSQL-GATE-05/SYN13_COVERAGE_SUMMARY.env`
  - `docs/planning/native_sql/gates/NSQL-GATE-05/CAPABILITY_MATRIX_SUMMARY.env`
- Gate-05 regeneration command: `tools/compliance/native_sql_gate05_scope_promotion.sh`
- Parser/runtime gap matrix: `docs/audit/PARSER_V3_MISSING_PARTIAL_MATRIX_BETA_0_1_0.md`
- Partial-language-guide input list: `docs/audit/V3_PARTIAL_ITEMS_WORKPLAN_INPUT_2026-02-19.md`
