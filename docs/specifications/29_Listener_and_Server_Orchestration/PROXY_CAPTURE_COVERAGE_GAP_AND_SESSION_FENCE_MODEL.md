Status: reconstructed_required

# Proxy Capture Coverage Gap and Session Fence Model

## Purpose

This document defines how the proxy or listener lane represents coverage gaps when observing donor traffic for migration or compatibility mediation.

## Canonical Rule

Proxy observation is never assumed complete unless coverage proof exists. Any uncovered session class, transport path, or out-of-band mutation path creates an explicit coverage gap.

## Coverage Dimensions

Coverage shall be evaluated across:

- transport family
- session class
- authenticated principal class
- DDL path
- DML path
- administrative mutation path
- bulk import or export path

## Session Fence Rule

Where proxy capture is used for migration correctness, the system shall be able to establish a session fence identifying:

- sessions fully captured by the proxy
- sessions known to bypass the proxy
- sessions of unknown capture state

## Gap Classes

Coverage gaps shall be classified as:

- `NO_GAP_PROVEN`
- `KNOWN_BYPASS_PATH`
- `PARTIAL_SESSION_COVERAGE`
- `UNKNOWN_COVERAGE`

Any value other than `NO_GAP_PROVEN` downgrades migration certainty.

## Publication Rules

The runtime shall publish:

- current gap class
- basis for the class
- session-fence state
- whether cutover may proceed automatically, with warning, or not at all

## Cutover Interaction

If coverage gaps exist, the migration lane shall require stronger quiesce, stronger divergence scanning, or operator override according to section 39.

## Non-Guarantees

This file does not require the listener to become a universal donor surveillance surface. It requires honest capture-boundary publication.
