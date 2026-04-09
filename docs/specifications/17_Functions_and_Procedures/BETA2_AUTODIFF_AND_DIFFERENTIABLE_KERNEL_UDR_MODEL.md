# Beta 2 Autodiff And Differentiable Kernel UDR Model

## Purpose

This document defines the automatic-differentiation and differentiable-kernel
UDR family used for calibration, optimization, scientific fitting, and
machine-learning-adjacent analytical workflows.

This group uses `JAX` as a design reference for differentiation and compiled
kernel structure, but it does not authorize a Python runtime or a full JAX
environment inside ScratchBird.

## Owning package

- `sb_pkg_autodiff_udr`

## Dependencies

This package depends on:

- `sb_pkg_num_array_udr`
- `sb_pkg_expr_udr`
- `sb_pkg_symbolic_udr`
- `sb_pkg_opt_udr`

## Mandatory surfaces

The package shall provide:

- gradient of admitted scalar and vector expressions
- jacobian and hessian of admitted expressions
- reverse-mode and forward-mode differentiation for the admitted subset
- differentiable kernel compilation for admitted pure functions
- calibration-oriented helper routines that consume gradient information

## Required routine families

At minimum the following families shall exist:

- `sb_autodiff.grad(...)`
- `sb_autodiff.jacobian(...)`
- `sb_autodiff.hessian(...)`
- `sb_autodiff.compile_diff_kernel(...)`
- `sb_autodiff.calibrate_*`

## Example contract

```sql
select sb_autodiff.grad(
    expr => 'x^2 + 3*x + 1',
    variables => array['x'],
    values => struct_pack('x', 5.0)
);
```

## Operational rules

1. Autodiff admission is limited to pure, side-effect-free admitted functions.
2. Differentiation must fail closed when an operation has no admitted
   derivative rule.
3. Differentiable kernels shall participate in the same validation and cache
   policies as compiled expression kernels.
4. Higher-order derivative depth shall be bounded by policy.

## Explicit exclusions

- unrestricted tensor-compilation frameworks
- GPU-specific runtime as a baseline requirement
- general neural-network training stacks
