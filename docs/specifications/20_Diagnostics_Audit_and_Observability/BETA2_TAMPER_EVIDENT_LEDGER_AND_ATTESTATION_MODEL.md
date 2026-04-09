# Beta 2 Tamper Evident Ledger And Attestation Model

## Purpose

Define tamper-evident digest chains, verifier runs, and attestation exports for
protected datasets and administrative evidence surfaces.

## Governing rules

1. Ledger proof is derivative evidence over MGA truth, not replacement truth.
2. Digest chains must be deterministic and replayable.
3. Verification results are persisted and auditable.
4. Attestation export is explicit and versioned.

## Canonical metadata

- `sb_ledger_policy`
  - `policy_uuid`
  - `scope_class`
  - `scope_uuid`
  - `digest_family`
  - `anchor_policy`
  - `enabled`
- `sb_ledger_block`
  - `block_uuid`
  - `policy_uuid`
  - `sequence_no`
  - `from_commit_epoch`
  - `to_commit_epoch`
  - `digest_value`
  - `parent_digest`
- `sb_ledger_anchor`
  - `anchor_uuid`
  - `policy_uuid`
  - `block_uuid`
  - `anchor_kind`
  - `anchor_locator`
  - `created_at`
- `sb_ledger_verification_run`
  - `run_uuid`
  - `policy_uuid`
  - `from_sequence_no`
  - `to_sequence_no`
  - `status`
  - `failure_code`

## Ledger flow

1. Protected commit range closes.
2. Ledger worker derives canonical digest input.
3. One `sb_ledger_block` is emitted.
4. Anchor policy optionally exports digest proof.
5. Verifier runs may replay the chain at any later point.

## Refusal rules

- `LEDGER_SCOPE_UNKNOWN`
- `LEDGER_CHAIN_GAP`
- `LEDGER_DIGEST_MISMATCH`
- `LEDGER_ANCHOR_EXPORT_FAILED`

## Metrics

- blocks emitted
- anchor latency
- verification success rate
- digest mismatch count

## Example

```sql
create ledger policy finance_ledger on table finance.entries;
call sb_ledger.verify(policy_name => 'finance_ledger');
select * from sb_ledger.attestation_export(policy_name => 'finance_ledger');
```

## Cross-section requirements

- section `20` owns digest blocks, verifier output, and attestation exports
- section `24` owns scope binding metadata
- section `39` owns archive and replay interaction when protected data is moved
