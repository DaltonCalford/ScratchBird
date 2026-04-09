# Logical Rewrite Inventory and Trace

Status: current_authority

Current authoritative surfaces:
- `QueryCompilerV3::compileTrace(...)`
- compiler plan-profile support in `query_compiler_v3_optimizer_support.cpp`
- runtime-plan provenance fields and contract identifiers in `plan_payload.h`

## Current guarantees

- compile trace can expose normalized SQL context, SQL hash, AST hash, SBLR hash, and root opcode symbol
- runtime-plan payloads carry current provenance and diagnostics identifiers where populated

## Non-claims

- an exhaustive rewrite inventory for every optimizer pass
- a universal rewrite-trace manifest across all statements and all execution modes
