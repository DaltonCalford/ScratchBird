# Section 22 Decision Record

Status: current_authority

## Decisions

1. SBLR is the only canonical logical representation shared across parser families.
2. The v3 container family is authoritative for current implementation work.
3. Parsers own identifier normalization and committed-baseline catalog resolution before SBLR emission.
4. The engine must not infer missing durable catalog identity from raw names when SBLR requires UUID-bound objects.
5. Verifier enforcement is mandatory and sits on the execution-critical path.
6. Renderer fidelity is bounded by canonical SBLR semantics, not by original dialect formatting.
7. Forward evolution is additive only through explicit opcode and payload registry updates; silent interpretation changes are forbidden.

## Rejected alternatives

- engine-side late name resolution as the normal path
- per-parser private IRs handed directly to execution
- optional verifier enforcement
- dialect-specific opcode families that bypass the canonical registry
