# Portability Non Guarantees

This file owns the explicit exclusions for section 41.

## Portability non-guarantee matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| platform-neutral semantics | fail_closed | semantics may vary with bounded current host support and runtime modes | not complete host-independence |
| every-toolchain support | fail_closed | toolchain support remains bounded to explicit build evidence | not compiler-agnostic portability |
| every-deployment-target support | fail_closed | deployment support remains bounded to current surfaces | not universal cloud/container/on-prem parity |
| long-term lifecycle guarantees | fail_closed | lifecycle guarantees remain narrower than full product-policy commitments | not enterprise support matrix claims |

## Canonical rules

1. Portability exclusions must be stated aggressively where current proof is sparse.
2. Absence of a contradiction is not proof of portability.
3. Section 41 must keep unsupported targets fail-closed.

## Explicit non-guarantees

- no universal host portability guarantee
- no every-compiler support promise
- no complete lifecycle support matrix claim
