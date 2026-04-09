# Hash Index Specification

Status: current_authority

## Purpose

This document defines the equality-only hash index family.

## ScratchBird shipped type coverage

Current runtime routes the following index types through the hash family:

- `HASH`
- `REDIS_STRING`
- `REDIS_HASH`
- `REDIS_SET`
- `REDIS_HLL`

## MGA-first contract

- the hash family is a candidate finder for equality probes only
- heap/version truth is materialized before hash publication
- readers use the hash family to locate candidate TIDs and then apply MGA visibility
- multiple historical candidates for the same key may coexist until reclaim proof authorizes cleanup
- uniqueness, if required, is enforced by heap-visible conflict rules, not by trusting the bucket state alone

## Search and update rules

- range scans are not supported
- inserts add a candidate for the new heap version
- updates that change the indexed value perform logical old-value retire plus new-value publish
- delete cleanup waits for heap reclaim proof
- overflow buckets or chained pages are structural only

## Required optimizer metrics

The hash-family metrics packet shall include at minimum:

- bucket count
- load factor
- overflow chain depth
- equality probe candidate count
- duplicate density
- dead-entry debt
- MGA visibility reject rate
- bucket skew / hottest-bucket ratio
- metrics freshness and confidence
