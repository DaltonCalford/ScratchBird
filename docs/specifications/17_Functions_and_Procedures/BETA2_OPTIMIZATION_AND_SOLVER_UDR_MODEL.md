# Beta 2 Optimization And Solver UDR Model

## Purpose

This document defines the constrained optimization, planning, routing,
scheduling, and convex-model UDR family for ScratchBird.

This group is the ScratchBird-native replacement target for the highest-value
portions of `OR-Tools` and `CVXPY`.

## Owning package

- `sb_pkg_opt_udr`

## Mandatory surfaces

The package shall provide:

- linear programming
- mixed-integer programming
- bounded convex optimization for admitted problem classes
- assignment and matching
- scheduling
- routing for admitted bounded problem sizes
- network flow helpers
- model definition, solve, inspect, and explain surfaces

## Required routine families

At minimum the following routine families shall exist:

- `sb_opt.model_create(...)`
- `sb_opt.model_add_variable(...)`
- `sb_opt.model_add_constraint(...)`
- `sb_opt.model_set_objective(...)`
- `sb_opt.solve_lp(...)`
- `sb_opt.solve_mip(...)`
- `sb_opt.solve_schedule(...)`
- `sb_opt.solve_route(...)`
- `sb_opt.explain_solution(...)`

## Example contract

```sql
select *
from sb_opt.solve_lp(
    objective => json_object(...),
    constraints => json_array(...),
    variable_bounds => json_array(...)
);
```

## Execution rules

1. All solver entry points shall use the `bounded_solver` execution class.
2. Every solve request shall declare or inherit:
   - max runtime
   - max iterations or search nodes where applicable
   - memory ceiling
   - termination policy
3. The package shall expose structured solver diagnostics:
   - feasibility
   - optimality status
   - termination reason
   - gap metrics
   - iteration or node counts
4. Solver routines may not run as unbounded background jobs through the
   ordinary foreground function surface.

## Persistence rules

1. Optimization models may be stored as catalog-backed artifacts or structured
   payload blobs.
2. A stored model shall be immutable by version once accepted.
3. Solution artifacts shall be independently addressable from model artifacts.

## Explicit exclusions

- unrestricted external solver processes
- remote optimization services
- open-ended heuristic searches without configured budgets
