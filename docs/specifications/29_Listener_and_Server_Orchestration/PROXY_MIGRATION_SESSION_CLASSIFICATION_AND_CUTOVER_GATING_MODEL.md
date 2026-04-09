Status: reconstructed_required

# Proxy Migration Session Classification and Cutover Gating Model

## Purpose

This document defines how proxy-observed donor sessions are classified for migration safety and how that classification affects cutover gating.

## Canonical Rule

Proxy migration shall classify donor sessions before using them as evidence for cutover readiness. Session classes with unknown or partial coverage weaken cutover certainty.

## Session Classes

The canonical classes are:

- `FULLY_CAPTURED_DML`
- `FULLY_CAPTURED_DDL`
- `READ_ONLY`
- `BYPASS_UNKNOWN`
- `ADMIN_MUTATION`
- `BULK_OR_OUT_OF_BAND`

## Gating Rule

Automatic cutover readiness may consider only sessions classified as fully captured or explicitly fenced. Presence of `BYPASS_UNKNOWN`, `ADMIN_MUTATION`, or `BULK_OR_OUT_OF_BAND` sessions requires stronger divergence evidence or donor quiesce.

## Publication Rule

The migration status surface shall publish:

- active session counts by class
- unknown or bypass session presence
- current cutover gating state
- reason automatic cutover is allowed, warning-only, or refused

## Non-Guarantees

This file does not require the proxy to classify every donor product identically. It requires an explicit class model and cutover gate consequence.
