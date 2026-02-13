# Listener/Parser Pool Design Principles (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

This document captures the design principles for the listener/pool control plane.
Legacy "Y-Valve" references are normalized to this control plane.

## Core Design Principles

1. Minimal core: listener/pool only detects protocol and hands off sockets.
2. Parser ownership: protocol-specific logic lives entirely in parsers.
3. No SQL in the engine: parsers translate to SBLR; engine never parses SQL.
4. Strict protocol boundaries: parsers do not call each other.
5. Deterministic handoff: all socket transfers are explicit and audited.

## Compatibility Targets (Normative)

- PostgreSQL 16+ behavior
- MySQL 8.x behavior
- Firebird 5.x behavior
- TDS/MSSQL is not supported and MUST be rejected

## Responsibilities

Listener/Pool MUST:
- Detect protocol and reject unsupported protocols.
- Enforce connection limits and backpressure.
- Spawn and recycle parser workers.
- Emit metrics for all handoff and lifecycle events.

Parser MUST:
- Own wire protocol state machine for its dialect.
- Authenticate via engine using the IPC contract.
- Translate SQL to SBLR without embedding SQL in the engine.

Engine MUST:
- Execute SBLR with no SQL parsing.
- Enforce lock/GC/visibility semantics from V3 transaction specs.

## Non-Goals (V3)

- Cluster-aware listener coordination
- Protocol translation between dialects
- Legacy protocol versions

## Related Specs

- `docs/specifications/parser/v3/network/NETWORK_LAYER_SPEC.md`
- `docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md`
- `docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md`
