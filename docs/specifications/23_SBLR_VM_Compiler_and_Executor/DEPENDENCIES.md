# Dependencies

## Primary implementation dependencies

- section `21` parser and lowering surfaces
- section `22` `SBL3` container, opcode identity, payload maps, and validator rules
- optimizer core in current planner, join-ordering, index-family, statistics, selectivity, and plan-selection paths
- compiler integration through `QueryCompilerV3` and finalize support
- executor integration for runtime-plan consumption and explain output
- section `24` catalog and statistics exposure used by planning and artifact metadata

## Shared-boundary dependencies

- section `18` for index-family semantics and shared backend truth
- section `19` for security or capability epochs used in invalidation
- section `20` for diagnostics and observability surfaces
- section `35` for MGA transaction and recovery semantics affecting execution behavior

## Explicit non-dependencies

This section does not require a separate stable VM ABI, compute-capsule subsystem, or distributed scheduler subsystem to define current authority.
