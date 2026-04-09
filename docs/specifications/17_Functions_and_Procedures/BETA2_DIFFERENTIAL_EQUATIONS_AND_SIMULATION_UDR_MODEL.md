# Beta 2 Differential Equations And Simulation UDR Model

## Purpose

This document defines the differential-equation and simulation UDR family for
bounded ODE, SDE, and admitted PDE-class workflows inside ScratchBird.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `DifferentialEquations.jl`.

## Owning package

- `sb_pkg_diff_eq_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_sci_udr`
- `sb_pkg_prob_udr`
- `sb_pkg_units_udr`
- `sb_pkg_exact_math_udr`

## Mandatory surfaces

The package shall provide:

- initial-value problem definition
- parameter binding
- solver selection from an admitted bounded solver family
- ODE solve
- SDE solve for admitted stochastic models
- event and stopping-condition definition
- dense or sampled solution export
- sensitivity extraction for admitted models

## Required routine families

At minimum the following families shall exist:

- `sb_diff_eq.problem_ode(...)`
- `sb_diff_eq.problem_sde(...)`
- `sb_diff_eq.solve(...)`
- `sb_diff_eq.solution_sample(...)`
- `sb_diff_eq.solution_events(...)`
- `sb_diff_eq.sensitivity_*`

## Example contract

```sql
select *
from sb_diff_eq.solve(
    model_expr => 'dx/dt = alpha * x',
    initial_state => json_object('x', 10.0),
    parameters => json_object('alpha', 0.15),
    t_start => 0.0,
    t_end => 20.0,
    step_hint => 0.1
);
```

## Operational rules

1. Every solve shall declare:
   - solver family
   - tolerance policy
   - max steps
   - max runtime
   - memory ceiling
2. Adaptive solvers shall emit step count, reject count, and termination
   reason.
3. Stochastic solvers shall require explicit seed and random source policy.
4. Unit-aware problems shall reject inconsistent dimensions before solve.

## Explicit exclusions

- unrestricted PDE frameworks
- GPU-only simulation as a baseline requirement
- black-box external simulator processes
