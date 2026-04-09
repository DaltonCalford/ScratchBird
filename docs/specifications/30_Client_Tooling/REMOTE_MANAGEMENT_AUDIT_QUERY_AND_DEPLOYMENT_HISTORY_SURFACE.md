Status: reconstructed_required

# Remote Management Audit Query and Deployment History Surface

## Purpose

This document defines the canonical operator-facing inspection surface for remote-management instruction history, audit state, drift, and deployment results.

## Canonical Rule

Operators shall be able to inspect remote-management history without needing direct access to internal cluster tables or listener-local state. Inspection and mutation remain separate privileges.

## Required Inspection Classes

The tooling surface shall support deterministic inspection of:

- queued instructions
- assessed instructions
- applied instructions
- quarantined instructions
- per-target deployment history
- target drift
- capability snapshots relevant to decisions

## Required Output Fields

For every instruction or deployment row, the surface shall preserve:

- instruction identity
- submission source
- target identity
- lifecycle state
- latest assessment result
- latest apply result
- drift state
- audit timestamps
- operator or service principal attribution

## History Rules

The audit query surface shall preserve chronology. A later status row shall never erase the earlier lifecycle evidence needed to explain how the instruction reached its current state.

## Comparison Rules

The surface shall support comparison between:

- intended state
- cluster-recorded state
- target-local observed state

without requiring offline reconstruction.

## Privilege Rules

Inspection privilege may reveal deployment history and drift. It shall not imply permission to:

- apply or cancel instructions
- clear quarantine
- override assessment refusals
- mutate security-sensitive deployment targets

## Non-Guarantees

This file does not require one specific SQL or CLI syntax. It defines the required operator-facing information contract.
