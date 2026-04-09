# LSM-Tree Specification

Status: current_authority

## Purpose

This document defines the write-optimized LSM family used for append-heavy or compaction-managed index workloads.

## ScratchBird shipped type coverage

Current runtime exposes `LSM` as a distinct write-optimized family.

## MGA-first contract

- memtable and SSTable entries are candidate state, not visibility truth
- heap/version truth is materialized before LSM publication is considered committed
- tombstones and newer sequence versions coexist with older entries until compaction and heap reclaim proof allow removal
- compaction is a maintenance process only and must not invent correctness after a crash

## Read contract

- reads merge memtable and SSTable candidates in descending recency order
- the winning non-retired candidate must still pass MGA visibility
- tombstones suppress older candidates only when the associated heap/version truth and transaction state support that result

## Crash and recovery rule

- the LSM family is not backed by a WAL-authoritative recovery model
- if uncertain flush state or manifest state is detected, the family may be invalidated and rebuilt from authoritative heap truth

## Required optimizer metrics

The LSM-family metrics packet shall include at minimum:

- memtable size and occupancy
- immutable memtable count if present
- SSTable count by level
- level size and overlap debt
- bloom false-positive rate
- compaction debt
- candidate count per probe
- tombstone debt
- MGA visibility reject rate
- metrics freshness and confidence
