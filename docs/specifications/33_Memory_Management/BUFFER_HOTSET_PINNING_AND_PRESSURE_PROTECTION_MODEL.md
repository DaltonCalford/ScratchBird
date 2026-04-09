Status: reconstructed_required

# Buffer Hotset Pinning and Pressure Protection Model

## Purpose

This document defines how ScratchBird protects hot working sets in buffer memory while still obeying explicit pressure escalation.

## Canonical Rule

Frequently used hot working sets may be protected from ordinary eviction, but only through explicit pinning or protection classes. Hidden accidental persistence is non-conforming.

## Hotset Classes

The canonical hotset classes are:

- checkpoint-critical pages
- actively scanned or joined working pages
- resident index hot pages or segments
- metadata hotset required by current workload

## Pinning Rule

Hotset pinning shall preserve:

- object or page identity
- owning subsystem
- pin reason
- pin lifetime or review boundary
- pressure-demotion rule

## Pressure Protection Rule

Protected hotsets may resist ordinary eviction, but under higher pressure classes the runtime shall either:

- demote the hotset explicitly
- spill or degrade other less critical classes first
- refuse new work instead of silently discarding protected state

## Diagnostics Requirements

The runtime shall expose:

- pinned hotset bytes
- hotset object count
- demotion count
- protection-related refusal count

## Non-Guarantees

This file does not require every page to be pinnable. It requires explicit classification and accounting for protected hotsets.
