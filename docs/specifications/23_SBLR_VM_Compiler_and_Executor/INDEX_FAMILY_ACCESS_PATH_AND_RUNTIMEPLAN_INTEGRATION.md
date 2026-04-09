# Index Family Access Path and RuntimePlan Integration

Status: current_authority

Current authority:
- `src/optimizer/index_family_lowering.cpp`
- `src/optimizer/query_planner.cpp`
- `src/optimizer/statistics_manager.cpp`
- runtime-plan relation payload fields in `plan_payload.h`

## Current guarantees

- catalog index types are lowered into planner access families through current code-backed rules
- runtime-plan payloads record chosen scan family, physical family, exactness, visibility enforcement, queryability, and family capability metadata where populated
- shared-backend family truth from section `18` is reflected as planner-facing family lowering, not as independent physical-engine proof for every named family

## Non-claims

- complete independent runtime proof for every registry-exposed index family
- universal operator or planner parity across every named family
