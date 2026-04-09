# Normative Checklist: IBM Db2 DRDA Wire Protocol Adapter (Beta 2 Target)

## Purpose
Define the deterministic implementation contract for `IBM Db2` protocol emulation in the parser layer using `DRDA` and `DDM`.

## Scope
- `DDM` framing and code-point handling.
- `DRDA` manager negotiation, authentication, relational-database access, SQL execution, and query-block flows.
- Parser-to-engine mapping and deterministic donor-visible result and error encoding.

## Hard Invariants
1. Engine never speaks `DRDA/DDM` and never parses donor SQL directly.
2. Parser implements all `Db2` frame, object, code-point, and reply-message decode or encode behavior.
3. Parser maps donor requests to canonical AST, SBLR, and UUID-bound operations only.
4. Parser profile controls `Db2` family capability exposure and reject paths.
5. Parser never executes `Db2` semantics locally.

## Beta 2 Wire Profile
- outer carrier: `DDM` objects over `TCP/IP`
- relational protocol family: `DRDA Version 4`
- required startup and security flow includes:
  - `EXCSAT`
  - `ACCSEC`
  - `SECCHK`
  - `ACCRDB`
- required SQL execution flow includes:
  - direct statement execution
  - prepared or package-backed statement execution
  - query open and continue flows
  - commit and rollback sync-control flows

## Manager Negotiation And Security Contract
1. Parser must implement initial `EXCSAT` exchange and `DDM` manager-level capability negotiation.
2. Parser must implement `ACCSEC` and `SECCHK` request/reply sequencing with deterministic failure handling.
3. Parser must implement `ACCRDB` only after successful security exchange and only for the bound logical database root.
4. Parser must initialize session and package state only after `ACCRDB` success.

## Required Command And Reply Families
- connection and manager negotiation:
  - `EXCSAT`
  - `EXCSATRD`
  - `ACCSEC`
  - `ACCSECRD`
  - `SECCHK`
  - `SECCHKRM`
  - `ACCRDB`
  - `ACCRDBRM`
- statement and package lifecycle:
  - `PRPSQLSTT`
  - `EXCSQLSTT`
  - package or section metadata required to bind prepared execution deterministically
- query lifecycle:
  - `OPNQRY`
  - `CNTQRY`
  - `CLSQRY`
  - row-data and query-data reply objects
- transactional control:
  - `SYNCCTL`
  - commit and rollback reply paths

## Request Mapping Contract
- direct execution requests:
  - decode statement payload
  - capability gate
  - UUID bind
  - emit SBLR
- prepared or package-backed requests:
  - persist parser-side package and section metadata deterministically
  - bind parameter metadata and execution shape deterministically
  - map execute and fetch flows to canonical engine requests
- query continuation:
  - preserve donor block-fetch and cursor continuation semantics inside the parser boundary

## Result And Error Mapping Contract
- Parser MUST map engine metadata to donor-visible descriptor objects deterministically.
- Parser MUST preserve reply ordering for open-query, continue-query, and end-of-query paths.
- Parser MUST map parser and engine failures to deterministic `Db2` SQLCODE or SQLSTATE-visible error payloads.
- Parser MUST sanitize discoverability-sensitive failures and never leak ScratchBird internal text or UUIDs.

## Implementation Checklist

### DB2W00 DDM Framing And Code Points
- [ ] Implement `DDM` object framing readers and writers.
- [ ] Validate length, code-point, and object nesting deterministically.
- [ ] Reject malformed or truncated objects without desynchronizing the session.

Pass condition:
- `DDM` framing is deterministic and corruption-safe.

### DB2W01 Manager Negotiation And Security
- [ ] Implement `EXCSAT` and negotiated manager handling.
- [ ] Implement `ACCSEC` and `SECCHK` success and failure flows.
- [ ] Implement `ACCRDB` only after security success and bound-root validation.

Pass condition:
- startup, auth, and database-access negotiation are state-safe and deterministic.

### DB2W02 Statement And Package Lifecycle
- [ ] Implement direct statement execution mapping.
- [ ] Implement prepared or package-backed statement lifecycle tracking.
- [ ] Persist parser-side package or section metadata deterministically for one session only.

Pass condition:
- statement execution and package state are protocol-correct and deterministic.

### DB2W03 Query Open And Continuation
- [ ] Implement open-query mapping and reply sequencing.
- [ ] Implement query continuation and end-of-query handling.
- [ ] Enforce deterministic cursor-state transitions and block-fetch behavior.

Pass condition:
- query result flow is protocol-correct for open, continue, completion, and error paths.

### DB2W04 Transaction Control
- [ ] Implement `SYNCCTL`-driven commit and rollback paths.
- [ ] Map transactional state to canonical engine control envelopes.
- [ ] Reject invalid transactional sequencing deterministically.

Pass condition:
- transaction control is deterministic and donor-compatible.

### DB2W05 Result And Error Rendering
- [ ] Implement donor-visible descriptor and row-data rendering from canonical results.
- [ ] Map parser and engine failures to deterministic `Db2` error payloads.
- [ ] Ensure no raw ScratchBird-only text leaks onto the `Db2` wire.

Pass condition:
- result and error rendering are complete, deterministic, and discoverability-safe.

## Negative Requirements
- Parser MUST NOT execute SQL or package logic locally.
- Parser MUST NOT expose catalog data outside the bound database root.
- Parser MUST NOT depend on external `Db2` client libraries to speak `DRDA`.

## Conformance Gates
- `P28-DB2W-GATE-01`: `DDM` framing and code-point validation tests pass.
- `P28-DB2W-GATE-02`: `EXCSAT`/`ACCSEC`/`SECCHK`/`ACCRDB` startup tests pass.
- `P28-DB2W-GATE-03`: statement, package, and query lifecycle tests pass.
- `P28-DB2W-GATE-04`: transaction, result, and error mapping tests pass.

## Evidence Artifacts
- `docs/specifications/work/conformance/wire/db2/DDM_FRAMING_RESULTS.json`
- `docs/specifications/work/conformance/wire/db2/STARTUP_AND_SECURITY_RESULTS.csv`
- `docs/specifications/work/conformance/wire/db2/STATEMENT_AND_PACKAGE_RESULTS.csv`
- `docs/specifications/work/conformance/wire/db2/QUERY_AND_CURSOR_RESULTS.csv`
- `docs/specifications/work/conformance/wire/db2/ERROR_MAPPING_RESULTS.csv`

## Cross-Section Links
- `docs/specifications/28_Parser_Implementations/BETA2_EMULATION_FAMILY_DB2_MODEL.md`
- `docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-04-03)
- This file is a target-state checklist, not stand-alone proof of shipped `Db2` parity.
- Current source-backed authority comes from the captured `DRDA Version 4` standard and IBM trace material, not from a shipped ScratchBird `Db2` parser implementation.
- Runtime parity remains bounded until family-local parser, compiler UDR, emulation UDR, and conformance artifacts exist.
