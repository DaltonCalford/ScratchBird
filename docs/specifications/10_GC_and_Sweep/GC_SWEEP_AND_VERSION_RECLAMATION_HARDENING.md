# GC Sweep and Version Reclamation Hardening

Status: current_authority

## Purpose

Define the current hardened reclaim rules for MGA version cleanup, page-local repair, restart-safe progress publication, and coordinated heap-index retirement.

## Current hardening truth

The strongest code-backed hardening in this section is:
- canonical heap version-maturity scanning before reclaim
- explicit prune and reclaim API boundaries in HeapPage
- sweep-stage separation between page audit and reclaim
- persisted sweep-manifest validation before resume
- publication of sweep generation and repair-adjacent deferred work into observability

## Required controls proven now

- resumable generations through validated persisted sweep state
- stage separation between page audit and reclaim work
- explicit blocked-evidence publication when reclaim cannot proceed
- repair-adjacent publication through observability surfaces

## Current reclaim outcome model

Current code proves these outcome classes in behavior even where one centralized enum is not exposed:
- reclaim now when maturity and legality checks succeed
- defer because horizon or visibility rules are not yet satisfied
- defer because repair or blocked evidence requires publication first
- rewind or restart because persisted sweep state is incompatible with current generation or recovery state

## Restart compatibility

A persisted cursor may resume only when the persisted sweep manifest validates against the current sweep and recovery state. Otherwise sweep rewinds according to current validated resume logic instead of trusting stale persisted progress.

## Non-guarantees

- no claim is made here that one centralized reclaim-outcome enum already exists in code
- no claim is made here that every named control from older prose is exposed as one unified runtime control surface
- no claim is made here that cooperative cleanup and background sweep already expose one public family-neutral API
