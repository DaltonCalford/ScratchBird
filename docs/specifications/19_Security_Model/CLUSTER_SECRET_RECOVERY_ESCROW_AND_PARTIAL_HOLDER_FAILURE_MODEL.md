Status: reconstructed_required

# Cluster Secret Recovery Escrow and Partial Holder Failure Model

## Purpose

This document defines how cluster-secret recovery escrow interacts with partial shard-holder failure or loss.

## Canonical Rule

Partial failure of shard holders shall not silently downgrade the quorum model into single-node secret recovery. Recovery escrow is a bounded policy surface, not a bypass of the shard and quorum model.

## Failure Classes

The canonical partial-holder failure classes are:

- holder unavailable
- holder lost
- holder compromised
- holder stale-version only

## Recovery Escrow Rule

Recovery escrow may participate only when:

- the escrow role is explicitly part of policy
- escrow participation is audited
- quorum requirements remain satisfied according to the adjusted emergency policy

## Emergency Policy Rule

Emergency policy may alter quorum or holder classes only through an explicit emergency transition record that preserves:

- old policy
- temporary emergency policy
- reason for transition
- approver set
- expiry or review boundary

## Non-Guarantees

This file does not permit silent secret recovery through one surviving holder. It defines bounded emergency and escrow participation.
