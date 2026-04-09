# Beta 2 Memory Optimized OLTP Lane And Compiled Kernel Model

## Purpose

Define the native high-rate OLTP lane for hot tables, hot keys, resident row
windows, and compiled kernels over current Beta 2 OLTP substrate.

## Governing rules

1. MGA truth remains authoritative.
2. The memory-optimized lane is a privileged execution and residency policy,
   not a second transaction model.
3. Table admission into the lane is explicit and reversible.
4. Compiled kernels are cached artifacts bound to one admitted plan shape.

## Canonical metadata

- `sb_hot_table_policy`
  - `table_uuid`
  - `residency_policy`
  - `hot_key_policy`
  - `compiled_kernel_policy`
  - `enabled`
- `sb_hot_partition`
  - `partition_uuid`
  - `table_uuid`
  - `key_range`
  - `resident_state`
  - `writer_role`
- `sb_compiled_kernel`
  - `kernel_uuid`
  - `table_uuid`
  - `plan_shape_id`
  - `arg_signature`
  - `status`

## Fast-path flow

1. Planner recognizes an admitted point read/write shape.
2. Table and key range are checked for hot-lane eligibility.
3. Compiled kernel or specialized interpreted fast path is chosen.
4. Commit-group batch apply and same-key suppression remain active.
5. Publication still follows ordinary MGA rules.

## Refusal rules

- `HOT_TABLE_POLICY_MISSING`
- `HOT_LANE_UNSUPPORTED_SHAPE`
- `HOT_KERNEL_INVALIDATED`
- `HOT_KEY_POLICY_REQUIRED`

## Metrics

- hot-lane hit rate
- compiled-kernel hit rate
- p99 latency by table
- hot-key suppression count

## Cross-section requirements

- section `25` owns residency and hot-lane policy
- section `36` owns fast-path shape recognition
- section `31` owns OLTP certification
