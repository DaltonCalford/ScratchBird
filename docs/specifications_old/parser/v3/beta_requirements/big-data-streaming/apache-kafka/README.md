# Apache Kafka Integration (Beta)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**[Back to Big Data & Streaming](../README.md)** | **[Back to Beta Requirements](../../README.md)**

This directory defines ScratchBird's Kafka integration for **broadcasting**
change/audit events and **consuming** Kafka streams for ingestion and migration.

## Specifications

- **[SPECIFICATION.md](SPECIFICATION.md)** - Kafka broadcaster + client design

## Summary

ScratchBird integrates with Kafka in three primary roles:
1. **Broadcaster (Source):** publish change events and audit streams.
2. **Client (Sink):** consume Kafka topics for ingestion, migration, or replay.
3. **Connectors:** optional Kafka Connect source/sink adapters.

## Status
Authoritative (V3)

