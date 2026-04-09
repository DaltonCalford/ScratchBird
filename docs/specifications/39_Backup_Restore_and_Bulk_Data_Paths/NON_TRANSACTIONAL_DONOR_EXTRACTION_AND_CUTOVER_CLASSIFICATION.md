# Non Transactional Donor Extraction and Cutover Classification

Status: reconstructed_required

## Purpose

This file defines the required migration and extraction model when the donor system lacks strong transactional guarantees or natural replication support.

## Donor weakness classes

- `statement_consistent_only`
- `weak_snapshot`
- `eventual_only`
- `no_reliable_change_stream`
- `no_forced_transaction_boundary`

A donor may fall into more than one weakness class.

## Required extraction controls

For a weak donor, the migration runtime must record:
- extraction window identity
- chunk or partition boundaries
- ordering key or watermark rule
- duplicate and late-arrival handling rule
- verification pass identity
- unresolved drift count
- cutover readiness classification

## Cutover classes

- `not_ready`
- `copy_complete_verification_pending`
- `verification_failed`
- `verification_passed_manual_cutover_required`
- `promotion_ready`

## Non-negotiable rule

Weak-donor cutover must never be represented as equivalent to native MGA commit publication. Promotion into ScratchBird authority happens only after validation and explicit cutover under ScratchBird transaction rules.
