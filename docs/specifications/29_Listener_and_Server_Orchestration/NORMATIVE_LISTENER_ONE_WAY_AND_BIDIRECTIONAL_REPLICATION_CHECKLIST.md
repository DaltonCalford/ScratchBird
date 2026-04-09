# Normative Listener One-Way and Bidirectional Replication Checklist

## Current status

One-way and bidirectional replication runtime is not implemented in the shipped
section `29` listener/server path.

## Required interpretation

- This document is not current implementation authority.
- No replication cursor, apply worker, conflict arbitration, or split-brain
  runtime may be inferred from section `29`.
- Any replication implementation requires separate canonical promotion before
  it can be claimed as shipped listener behavior.
