# Cardinality Statistics and Cost Model Governance

Status: current_authority

Current authority:
- `src/optimizer/statistics_manager.cpp`
- `src/optimizer/selectivity_estimator.cpp`
- `src/optimizer/vnext_plan_selection.cpp`
- `src/optimizer/query_planner.cpp`

## Current guarantees

- statistics refresh and confidence classification logic exist
- selectivity estimation exists for current predicate and join shapes the code handles
- plan-selection scoring inputs, histogram helpers, and plan-hash serialization support exist
- runtime-plan fields for rows, cost, confidence, and calibration or profile identifiers exist where populated

## Non-claims

- a universal cost-model governance framework across every query track
- complete learned-feedback or self-tuning closure
- full donor-style statistics policy claims for every family and workload
