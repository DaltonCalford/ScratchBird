# Full-Text and Inverted Index Specification

Status: current_authority

## Purpose

This document defines the inverted/posting-list family used for text, token, sparse, wildcard, and related inverted surfaces.

## ScratchBird shipped type coverage

Current runtime routes the following index types through the inverted family:

- `FULLTEXT`
- `INVERTED`
- `MONGODB_WILDCARD`
- `MONGODB_ENCRYPTED_RANGE`
- `NEO4J_TEXT`
- `CASSANDRA_SASI`
- `CASSANDRA_SAI`
- `TRIE`
- `NGRAM`
- `SPARSE_INVERTED`
- `SPARSE_WAND`
- `MINHASH_LSH`

## MGA-first contract

- posting lists contain candidate row identifiers only
- token extraction occurs from committed row content or visible build snapshots
- updates publish new postings after heap/version materialization
- obsolete postings remain cleanup candidates until heap reclaim proof succeeds
- search ranking or sparse scoring never overrides MGA visibility

## Search contract

- the inverted family produces candidate sets, postings, or scored candidates
- text and sparse planners may rank or prune candidates
- final acceptance still requires row fetch plus MGA visibility recheck
- any post-filter or exact recheck rate must be surfaced to the optimizer

## Maintenance and cleanup rules

- stale postings may coexist with new postings until reclaim proof allows removal
- posting-list compaction, segment merge, and tombstone removal are maintenance actions only
- cleanup must preserve token-to-row lineage until derivative evidence and reclaim proof are satisfied

## Required optimizer metrics

The inverted-family metrics packet shall include at minimum:

- token count
- document or row coverage count
- posting-list length distribution
- top-token skew
- candidate count per probe class
- post-filter or exact recheck rate
- dead-posting debt
- MGA visibility reject rate
- ranking calibration freshness where scoring is used
- metrics freshness and confidence
