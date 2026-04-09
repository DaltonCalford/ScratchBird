# Evidence Expectations

## General rule

Every ticket in this package must produce:

- code truth evidence
- benchmark or focused performance evidence
- correctness evidence
- tracker updates

## Ticket-level evidence requirements

### Write-path tickets

- focused unit or integration proofs for the new write path
- benchmark rerun showing movement in:
  - `load`
  - `bulk_insert_select`
  - `bulk_update_with_case` where applicable
- per-table load evidence for `customers`, `products`, `orders`, and
  `order_items`

### Prepared-query ticket

- prepared point select evidence
- prepared point update evidence
- prepared micro-batch insert evidence
- prepared bundle invalidate and rebuild evidence
- result-cache hit and miss evidence for cacheable prepared select

### Secondary-read and join tickets

- runtime-plan evidence proving the specialized path family was selected
- fallback or refusal evidence when a family remains illegal
- benchmark movement for the join-heavy processes

### Upper-stage operator tickets

- runtime-plan evidence proving incremental sort, structured-key hash, or
  vectorized upper-stage execution
- spill or workfile evidence where pressure applies
- benchmark movement for aggregate, distinct, and window processes

### Parallel and locality ticket

- runtime-plan evidence proving legal serial and parallel candidates were
  compared
- worker-count and grant evidence
- locality binding evidence or canonical refusal evidence

### Final closeout ticket

- clean build and full test pass
- pinned binary benchmark artifact root
- donor comparison summary
- final tracker snapshot

## Waiver rule

Waivers are allowed only when:

- a non-negotiable invariant blocks the donor technique
- the evidence proves the invariant, not just implementation delay
- the residual benchmark delta is recorded against the exact tracker row
