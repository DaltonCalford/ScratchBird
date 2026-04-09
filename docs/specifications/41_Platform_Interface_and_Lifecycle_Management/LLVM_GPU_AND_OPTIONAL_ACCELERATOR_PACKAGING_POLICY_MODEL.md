Status: reconstructed_required

# LLVM GPU and Optional Accelerator Packaging Policy Model

## Purpose

This document defines the packaging and deployment policy for optional LLVM, GPU, and other accelerator support.

## Canonical Rule

Accelerator support is optional and policy-gated. Packaging may include or omit LLVM and GPU support without changing engine correctness or the canonical ownership model.

## Packaging Variants

The canonical packaging variants are:

- no LLVM and no GPU support
- LLVM-capable but GPU-disabled
- LLVM-capable plus GPU-capable
- policy-disabled accelerator build using the same core engine library

## Admission Rule

A packaged accelerator capability becomes usable only when:

- the package contains the required support
- platform discovery succeeds
- runtime policy admits use
- memory and compatibility checks succeed

## Optionality Rule

Omitting accelerator support from a package is conforming. Any deployment automation, runbook, or benchmark comparison shall record whether the package included the optional accelerator surfaces.

## Non-Guarantees

This file does not require every product package to ship LLVM or GPU support. It requires explicit packaging-policy disclosure and optionality.
