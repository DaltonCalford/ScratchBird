# Columnstore Index Specification

Status: current_authority

## Purpose

This document defines the columnar projection family used for scan acceleration, aggregation support, and summary pruning.

## ScratchBird shipped type coverage

Current runtime exposes `COLUMNSTORE` as a distinct projection family with specialized scan paths.

## MGA-first contract

- columnstore segments are projection aids, not visibility truth
- row identifiers in projection groups must resolve back to heap/version truth
- inserts and updates append new projection state after heap/version materialization
- deleted or obsolete projection rows remain until reclaim proof authorizes compaction
- segment min/max, dictionaries, and deleted bitmaps are derivative structures only

## Search contract

- segment-level pruning may use min/max, dictionaries, null maps, or deleted bitmaps
- any surviving row identifier must pass MGA visibility before contributing to results
- aggregate pushdown and scan reduction must record false-positive and visibility-reject behavior for planner calibration

## Required optimizer metrics

The columnstore-family metrics packet shall include at minimum:

- row group count
- rows per group distribution
- compression ratio by encoding class
- min/max prune ratio
- deleted-row density
- scan amplification after pruning
- MGA visibility reject rate
- compaction debt
- metrics freshness and confidence
