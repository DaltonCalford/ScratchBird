# Planner Front Door and Statement Planning API

Status: current_authority

Current authority:
- `include/scratchbird/optimizer/query_planner.h`
- `src/optimizer/query_planner.cpp`

## Current implemented contract

Current section authority includes:
- `PlannerStatementKind`
- `StatementPlanRequest`
- `StatementPlanResult`
- `QueryPlanner::planStatement(...)`
- `buildSelectPlanImpl(...)`

## Current boundaries

- the planner front door is real and current
- select planning and runtime-plan production are current proof
- broader universal planning for every statement family is not claimed unless directly backed by the live code path

## Negative requirements

- do not claim closed statement-family parity beyond the proven front door
- do not infer a separate planner API generation or RPC layer not present in current code
