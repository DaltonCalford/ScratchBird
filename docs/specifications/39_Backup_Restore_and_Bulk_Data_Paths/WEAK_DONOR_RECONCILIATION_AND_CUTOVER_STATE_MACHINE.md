# Weak Donor Reconciliation and Cutover State Machine

## Scope

This file defines the reconstructed-required state machine for migrating from a
weak or non-native donor whose transactional or replication semantics cannot be
trusted to match ScratchBird's MGA model directly.

## Governing rule

Weak-donor migration is not treated as native replication.

Canonical rule:

- donor weakness must be classified explicitly
- reconciliation must be ordered
- cutover must be fail-closed

## Weak donor classes

This state machine applies to donors that are any of:

- non-transactional
- only weakly transactional
- lacking natural replication
- lacking authoritative transaction fences compatible with ScratchBird

## State machine

The canonical migration states are:

- `DISCOVER`
- `CLASSIFY_DONOR`
- `SELECT_EXTRACTION_MODE`
- `BASELINE_EXTRACT`
- `RECONCILE_DRIFT`
- `ASSESS_CUTOVER`
- `QUIESCE_OR_PROXY`
- `CUTOVER`
- `POST_CUTOVER_VERIFY`
- `COMPLETE`
- `BLOCKED`
- `QUARANTINED`

## Ordered procedure

The canonical procedure is:

1. discover donor identity and connector profile
2. classify donor capability
3. choose an authorized extraction mode
4. perform baseline extract
5. measure unresolved drift
6. repeat reconciliation until drift class is within allowed cutover boundary
7. assess cutover readiness
8. if donor cannot be safely quiesced, require explicit proxy or bounded
   acceptance path
9. perform cutover
10. run post-cutover verification
11. complete or quarantine

## Weak-donor cutover rule

Weak donors do not receive silent equivalence with native transactional donors.

Canonical rule:

- if unresolved drift remains above policy, cutover is blocked
- if donor quiesce cannot be achieved and no bounded proxy path exists, cutover
  is blocked
- if verification fails after cutover, the migration lane enters quarantined or
  rollback procedure according to policy

## Relationship to proxy mode

Proxy mode is a bounded control strategy, not a proof of transactional parity.

Canonical rule:

- proxy mode may be used to bridge donor weakness
- proxy mode does not erase donor capability classification
- post-cutover status must still record that the donor was weak and how drift
  was reconciled

## MGA boundary

ScratchBird remains MGA-authoritative during and after migration.

Canonical rule:

- donor-side weakness never relaxes ScratchBird transaction, visibility, or
  publication rules
- cutover acceptance is based on ScratchBird-side verified state, not donor log
  folklore

## Required evidence

The migration lane must remain able to produce, at minimum:

- donor capability class
- extraction mode class
- unresolved drift summaries
- cutover readiness decision
- cutover execution time
- verification result

## Fail-closed rules

The migration lane shall not:

1. classify a weak donor as native-equivalent without explicit canonical rule
2. perform cutover while unresolved drift remains above policy
3. claim success without post-cutover verification
4. hide donor weakness from operator-facing status surfaces
