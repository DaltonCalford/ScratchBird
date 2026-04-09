Status: current_authority

# JIT Equivalence, Scope, and Tiering Certification Model

## Purpose

This file defines the certification lane for JIT semantic equivalence, scope
eligibility, and hotness-tiering behavior.

## Required certification scenarios

The current certification lane already covers:

1. VM/native differential corpus equivalence
2. unknown-surface VM fallback
3. explicit compile rejection for non-routine surfaces
4. hotness-below-threshold VM execution
5. threshold-triggered compile queue promotion
6. queue saturation deterministic VM fallback
7. duplicate queue suppression
8. unsupported opcode-family rejection

## Equivalence gate

The equivalence gate requires that, for the covered corpus:

1. VM and JIT-capable execution both succeed
2. both produce result sets
3. result shape matches
4. result value matches

## Tiering gate

The tiering gate requires:

1. below-threshold requests remain on the VM path
2. threshold promotion may queue compile but must not break current execution
3. queue saturation remains deterministic and VM-safe
4. duplicate requests do not duplicate compile-queue entries

## Metrics gate

The tiering certification lane also requires validation of:

1. compile-queue enqueued count
2. compile-queue duplicate count
3. compile-queue current depth
4. compile-queue max depth
5. object-local total dispatch count

## Unsupported-scope and unsupported-opcode gate

The current certification lane requires:

1. unknown scope never enters native selection
2. explicit compile on non-routine scope returns `INVALID_ARGUMENT`
3. unsupported opcode family returns `NOT_SUPPORTED`

## Interpretation rule

This certification lane proves that hotness and queueing change when native
artifacts are built, not what the statement means.

## Reconstructed required expansion

The rebuild requires future differential corpus growth for:

1. package members
2. triggers
3. procedures
4. functions
5. more complex expression and control-flow families
