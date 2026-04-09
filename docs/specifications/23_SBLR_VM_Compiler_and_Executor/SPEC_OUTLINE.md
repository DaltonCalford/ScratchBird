# Section 23 Specification Outline

## Objective

Define the implementation-ready current compiler, planner, plan-payload, cache, and executor-integration contract for ScratchBird SBLR execution.

## Primary implementation lanes

- planner front door and statement planning API
- optimizer architecture and pass pipeline actually exercised by current code
- join search, access-path ordering, and family lowering
- statistics, selectivity, and cost-governance inputs
- runtime-plan payload serialization and explain-facing evidence
- plan-cache keying, invalidation, and bounded result-cache edges
- parse, emit, finalize, and bytecode validation integration
- execution diagnostics, memory-budget inputs, and spill-planning metadata

## Explicit unsupported-boundary files

- `NORMATIVE_ENGINE_PLAN_AND_EXECUTION_CHECKLIST.md`
- `NORMATIVE_P0_PLAN_AND_EXECUTION_OPTIMIZATION_CHECKLIST.md`
- `NORMATIVE_P1_DISTRIBUTED_READ_CACHE_AND_TELEMETRY_CHECKLIST.md`
- `NORMATIVE_P2_COST_AWARE_SCHEDULER_AND_TIEBREAK_CHECKLIST.md`
- `COMPUTE_CAPSULE_OBJECT_AND_ARTIFACT_MODEL.md`
- `BULK_LOAD_AND_COPY_EXECUTION_CONTRACT.md`
- `NATIVE_COMPILATION_AND_ARTIFACT_LIFECYCLE.md`

Those files are not current implementation authority unless explicitly promoted.
