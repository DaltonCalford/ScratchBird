# Section 00 Test Contract

Status: current_authority

## Required proof classes

1. execution-path gate
   - prove parser front doors do not become engine execution authorities
2. identity contract test
   - prove shared durable ID remains UUIDv7-backed across audited subsystems
3. MGA governance test
   - prove OIT and OAT and OST and wraparound behavior remain MGA-derived
4. lowering conformance test
   - prove Firebird, V3, PostgreSQL, and MySQL lowering paths compile to SBLR
5. fail-closed lowering test
   - prove lowering failure aborts rather than silently bypassing SBLR
6. artifact provenance test
   - prove source dialect, SBLR bytecode, and native artifact metadata persist correctly

## Proof obligations

- every section-00 claim must map to at least one concrete code anchor and one test or gate surface where possible
- parser-bypass negative proof is mandatory for execution-path governance
- anti-WAL and MGA-only proof is mandatory for recovery governance
- durable identity drift must fail closed

## Non-guarantees

- no claim is made here that all existing tests were exhaustively re-audited in this pass
- no claim is made here that section 00 already has one dedicated unified gate file
- no claim is made here that downstream sections already inherit these proof obligations without explicit mapping
