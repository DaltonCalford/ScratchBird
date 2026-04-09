# SP-GiST Specification

Status: current_authority

## Purpose

This document defines the space-partitioned search-tree family for radix, quad, trie-like, and related partitioned routing workloads.

## ScratchBird shipped type coverage

Current runtime exposes `SPGIST` as a distinct partitioned-tree family.

## MGA-first contract

- SP-GiST routes to candidate leaves only
- node labels, prefixes, and partitions are structural routing aids
- exact leaf acceptance and MGA visibility remain authoritative
- cleanup of stale leaves waits for heap reclaim proof

## Search contract

- inner-consistent routing may visit multiple partitions
- leaf-consistent results may require operator-specific exact recheck
- visibility rejects must be tracked separately from partition false positives

## Required optimizer metrics

The SP-GiST-family metrics packet shall include at minimum:

- tree depth
- partition fanout distribution
- degenerate `all_the_same` rate
- candidate count per probe
- exact recheck rate where applicable
- dead-entry debt
- MGA visibility reject rate
- metrics freshness and confidence
