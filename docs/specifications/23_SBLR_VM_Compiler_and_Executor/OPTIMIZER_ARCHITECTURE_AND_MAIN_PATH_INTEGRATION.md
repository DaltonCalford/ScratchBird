# Optimizer Architecture and Main Path Integration

Status: current_authority

Current authoritative optimizer surface:
- `query_planner.cpp`
- `join_ordering.cpp`
- `index_family_lowering.cpp`
- `statistics_manager.cpp`
- `selectivity_estimator.cpp`
- `vnext_plan_selection.cpp`
- compiler support in `query_compiler_v3_optimizer_support.cpp`

## Current guarantees

- real access-path selection and runtime-plan formation
- real bounded join search and legality checks
- real family lowering from catalog index metadata into planner access families
- real selectivity and statistics-backed cost inputs
- real plan selection and profile support for optimizer-aware compilation

## Non-claims

- universal optimizer coverage across every statement family
- donor-style exhaustive pass inventory claims
- full distributed scheduler or cluster-aware optimization parity
