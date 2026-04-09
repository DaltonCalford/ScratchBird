# Bitmap Index Specification

Status: current_authority

## Purpose

This document defines the low-cardinality bitmap family used for equality-style candidate reduction.

## ScratchBird shipped type coverage

Current runtime routes the following index types through the bitmap family:

- `BITMAP`
- `NEO4J_LOOKUP`
- `REDIS_BITMAP`

## MGA-first contract

- bitmaps map values to candidate row identifiers only
- heap/version truth remains authoritative
- updates and deletes create new heap/version truth first
- old bits remain logically retained until heap reclaim proof allows removal
- bitmap cleanup is downstream of heap reclaim and page verification

## Search contract

- bitmap probes may use single-value lookup or bitmap algebra across values
- resulting candidate row identifiers must be converted to row locators and rechecked for MGA visibility
- bitmap hits rejected by visibility must be counted for optimizer calibration

## Delete and reclaim rules

- removing a visible row from a bitmap is a cleanup action, not immediate visibility truth
- cleanup may clear bits, drop empty containers, or compact container formats only after reclaim proof
- derivative lanes such as write-after or temporal archive must already have satisfied required-before-prune rules if configured

## Required optimizer metrics

The bitmap-family metrics packet shall include at minimum:

- distinct value count
- average bitmap density
- container type mix
- average candidates per equality probe
- bitmap algebra reduction ratio
- dead-bit debt
- MGA visibility reject rate
- hottest-value density
- metrics freshness and confidence
