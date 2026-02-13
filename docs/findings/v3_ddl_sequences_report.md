# V3 DDL Sequences Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_SEQUENCES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 parses CREATE SEQUENCE with many options (START/INCREMENT/MIN/MAX/CACHE/CYCLE/OWNED BY), but **ALTER SEQUENCE is not parsed**.
- There is a **payload mismatch** between V3 emitter schema and executor’s expected byte layout for CREATE/ALTER/DROP SEQUENCE.
- SQL-style NEXT VALUE FOR / CURRENT VALUE FOR / SET sequence is not parsed in V3.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE SEQUENCE
[~] Parser supports IF NOT EXISTS, START/INCREMENT/MINVALUE/MAXVALUE/CACHE/CYCLE/OWNED BY.
[ ] Spec AS <data_type> not parsed.
[ ] Emitter uses schema payload (flags/path/start/increment/min/max/cache/cycle) and does **not** serialize OWNED BY or NO MIN/MAX flags.
[ ] Executor expects a different byte layout: name + flags byte + values + owned_by info + temp flags.

### ALTER SEQUENCE
[ ] No parser support in V3.
[~] Emitter supports AlterSequenceStmt, but no AST is produced.
[~] Executor supports alter sequence byte layout (flags + values).

### DROP SEQUENCE
[~] Parser supports IF EXISTS and CASCADE only; no RESTRICT handling.
[ ] Emitter uses schema payload; executor expects name + flags byte (cascade/if_exists).

### Sequence Usage Syntax
[ ] NEXT VALUE FOR / CURRENT VALUE FOR / SET <sequence> TO <value> not parsed in V3.

## Notes
- Align emitter/executor for sequence opcodes, and add ALTER SEQUENCE parsing + SQL-style sequence usage if required.
