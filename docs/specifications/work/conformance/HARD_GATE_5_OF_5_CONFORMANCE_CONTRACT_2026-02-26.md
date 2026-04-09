# Hard-Gate 5/5 Conformance Contract (2026-02-26)

Status: active execution contract  
Scope: independent audit scoring and release-gate truth controls

## 1) Purpose
Define fail-closed, evidence-driven scoring rules so a 5/5 result is only possible when conformance proof is complete and reproducible.

## 2) Hard-Gate Categories
All categories below must score at least 3/5 for a provisional pass and 5/5 for final conformance closure.

1. Wire protocol conformance
2. Transaction semantics conformance
3. Security runtime enforcement conformance
4. Parser boundary conformance (`v3` core vs emulation-only protocol adapters)
5. End-to-end SQL correctness conformance

If any hard-gate category is below 3/5:
- Overall result is `FAIL (not yet proven)`.

If any hard-gate category is below 5/5 for final closure:
- Overall result is `INCOMPLETE (not 5/5)`.

## 3) Scoring Scale (0-5)
1. `0`: absent.
2. `1`: stub/plumbing only.
3. `2`: implemented but no deterministic end-to-end proof.
4. `3`: at least one deterministic end-to-end proof path with expected output.
5. `4`: multiple deterministic proofs + negative tests.
6. `5`: broad deterministic proofs across required lanes + fail-closed reproducibility package + no unresolved hard contradictions.

## 4) Mandatory Evidence Quotas
### 4.1 Wire protocol
1. Frame-level request/response proof for SBWP, PostgreSQL, MySQL, and Firebird surfaces.
2. Golden trace index with checksums.
3. Negative packet/error-shape tests.

### 4.2 Transaction semantics
1. Cross-session matrix covering commit, rollback, savepoint, lock conflict, and deterministic error semantics.
2. Native + emulation lane artifacts with deterministic diff output.

### 4.3 Security enforcement
1. Runtime proof for row-level security enforcement.
2. Runtime proof for column-level permission enforcement.
3. Runtime proof for domain masking/encryption policy behavior.
4. Parity artifacts for native and emulation lanes where surface is supported.

### 4.4 Parser boundary
1. Explicit evidence that `v3` is the parser core path.
2. Explicit evidence that protocol lanes are emulation surfaces only.
3. Per-lane manifest artifacts (`RUN_MANIFEST.json` + parser boundary marker).

## 5) Reproducibility and Fail-Closed Rules
1. A one-command audit bundle runner must produce:
   - coverage map
   - evidence ledger
   - spot-check report
   - score output JSON
2. Missing mandatory artifacts force a hard fail.
3. Unknown/untested surfaces cannot be promoted above 2/5.

## 6) Gate Enforcement
1. CTest/CI gates must fail when hard-gate evidence artifacts are missing or non-passing.
2. Release signoff is blocked if any hard-gate category is below 5/5.

## 7) Output Artifacts for Signoff
1. `FRAME_CONFORMANCE_REPORT.md`
2. `GOLDEN_TRACE_INDEX.csv`
3. `TRANSACTION_TRUTH_MATRIX_REPORT.md`
4. `SECURITY_PARITY_MATRIX_REPORT.md`
5. `AUDITOR_RUNBOOK_BUNDLE.md`
6. `EVIDENCE_LEDGER.csv`
7. `SCORES.json`
