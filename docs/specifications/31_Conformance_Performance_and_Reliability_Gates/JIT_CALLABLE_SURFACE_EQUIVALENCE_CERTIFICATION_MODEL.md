Status: current_authority

# JIT Callable Surface Equivalence Certification Model

## Purpose

This file defines the certification lane for JIT callable-surface equivalence.

## Current certified callable surfaces

The current certification lane already includes:

1. function-style scalar expression equivalence
2. aggregate or procedure-like query equivalence
3. package-member style equivalence
4. trigger-adjacent table-activity equivalence

## Pass criteria

For each covered callable surface, the certification lane requires:

1. VM execution success
2. JIT-capable execution success
3. result-set presence where applicable
4. matching observable result values

## Certification boundary

This lane certifies callable-surface equivalence for the covered corpus only.

It does not certify:

1. every package construct
2. every trigger body shape
3. every procedural control-flow form
4. every side-effecting routine path

Additional callable-surface promotion requires additional differential corpus
coverage.
