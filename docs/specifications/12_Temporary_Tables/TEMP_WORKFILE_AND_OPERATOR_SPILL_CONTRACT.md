# Temporary Workfile and Operator Spill Contract

## Current authority

Current section `12` authority for spill behavior is limited to planner-side
surfaces.

Current code-backed authority includes:
- spill estimate fields in planner payloads
- spill policy parsing
- spill-disallow refusal for operators that would spill
- spill metadata in explain or output surfaces

## Planner spill surfaces

Current spill metadata fields include:
- `spill_expected`
- `spill_passes`
- `spill_bytes`
- `spill_policy`

These appear in the plan payload and related explain/output surfaces.

Current planner spill policy includes:
- allow spill
- disallow spill

When spill is disallowed, current planner refusal exists for at least:
- hash join
- merge join where the chosen path would spill
- aggregate operators that would spill
- window operators that would spill
- sort operators that would spill

## Explain and output surface

Current explain/output paths expose spill metadata and spill expectation. That
metadata is authoritative for current planner decisions.

## Explicit unsupported boundaries

Current section `12` does not certify:
- runtime workfile identity
- runtime workfile owner registry
- runtime workfile quotas or budgets
- spill artifact cleanup or restart behavior
- spill artifact diagnostics beyond planner or explain metadata
- a generalized temp-file subsystem for sort, hash, or materialize operators

## Negative requirements

- do not derive a runtime spill-file subsystem from planner metadata alone
- do not claim spill artifact durability or cleanup semantics not proven in
  current runtime code

## Competitive parity closure requirements

The competitive-performance parity package may not close while spill behavior
remains planner-only for benchmark-governed operators.

Before parity closure, ScratchBird shall promote bounded runtime workfile
authority for admitted spillable operators, including at least:

- runtime workfile identity
- runtime workfile owner registry
- runtime workfile budgets and admission
- cleanup and restart-safe lifecycle rules
- diagnostics for actual spill bytes, passes, and artifacts

This promotion is required for benchmark-governed:

- sort
- hash join
- merge join
- aggregate and distinct
- window
- materialize stages that remain dominant in benchmark execution

No benchmark-governed operator may remain limited to `SPILL_POLICY=DISALLOW`
refusal as its only mature answer if donor engines execute the same logical
shape through a bounded spill or workfile path.
