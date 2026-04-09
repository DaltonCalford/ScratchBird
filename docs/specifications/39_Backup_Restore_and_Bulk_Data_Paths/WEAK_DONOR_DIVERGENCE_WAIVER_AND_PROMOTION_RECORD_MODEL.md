Status: reconstructed_required

# Weak Donor Divergence Waiver and Promotion Record Model

## Purpose

This document defines the canonical waiver and promotion record required when a weak donor cannot reach zero-divergence certainty before cutover.

## Canonical Rule

If automatic promotion would otherwise be refused due to weak-donor divergence or uncertainty, any override requires an explicit waiver record. The waiver does not erase the divergence; it records accepted residual risk.

## Waiver Record Fields

The waiver record shall preserve:

- donor identity
- target identity
- divergence class
- uncertainty class
- missing guarantees or unresolved evidence
- accepting operator or policy actor
- scope of the waiver
- expiry or review boundary if policy requires one

## Promotion Linkage Rule

Every promoted target that relied on a divergence waiver shall retain a link to:

- the waiver record
- the snapshot or replay boundary used for promotion
- the final promotion decision

## Audit Rule

The waiver and resulting promotion shall remain queryable after cutover so later operators can distinguish:

- zero-divergence promotions
- warned promotions
- explicitly waived promotions

## Non-Guarantees

This file does not permit silent risky promotion. It defines the explicit record required when residual weak-donor risk is accepted.
