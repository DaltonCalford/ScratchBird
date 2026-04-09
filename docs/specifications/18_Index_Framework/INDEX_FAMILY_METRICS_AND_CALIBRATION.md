# Index Family Metrics and Calibration

Status: current_authority

## Purpose

This document defines the minimum family-local metrics required before the optimizer may rely on an index family.

## Ordered tree families

Ordered tree families shall expose at minimum:

- depth
- leaf page count
- branch page count
- average keys per leaf
- split frequency
- right-sibling chase frequency
- range-scan page amplification
- equality probe candidate count
- visibility reject rate

## Hash or equality families

Hash-like families shall expose at minimum:

- bucket count or equivalent partition count
- load factor
- overflow chain depth or equivalent spill measure
- equality probe candidate count
- duplicate density
- visibility reject rate

## Inverted or text-search families

Inverted families shall expose at minimum:

- token/posting count
- average posting-list length
- posting-list compression ratio
- token selectivity histogram or equivalent
- false-positive or post-filter rate
- visibility reject rate

## Spatial families

Spatial families shall expose at minimum:

- node depth or routing depth
- bounding-region overlap rate
- candidate set expansion rate
- exact recheck rate
- visibility reject rate

## Approximate or vector families

Approximate families shall expose at minimum:

- graph or routing depth
- candidate expansion parameter values
- average candidates examined
- post-filter exact-distance reject rate
- recall calibration signal
- visibility reject rate

## Summary or bitmap families

Summary, bitmap, or zone families shall expose at minimum:

- covered row or page range count
- skip ratio
- false-positive rate
- refresh lag
- visibility reject rate

## Calibration discipline

Every family shall maintain calibration data showing how often its predicted candidate counts and reject rates match observed runtime outcomes. The optimizer must degrade confidence when calibration drift exceeds the family threshold.
