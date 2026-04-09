# Remote Connector and Proxy Migration Catalog Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the catalog rows and policy objects that govern remote connectors, proxy routing, passthrough admission, migration jobs, reconciliation, and cutover assessment.

## Current Code-Backed Authority

Current code-backed substrate exists for:
- remote schema mapping
- remote passthrough policy
- shard or catalog-family migration records
- replication channel, member, origin, cursor, batch, apply-log, retry, and error catalog families
- failure detector and cluster policy substrate already captured elsewhere in section `24`

## Remote Connector Row

A remote connector row must capture at least:
- connector identity
- connector name
- remote engine identity
- endpoint or DSN reference
- credential or secret reference
- connector capability snapshot id
- last capability refresh time
- last validation state
- local owner or control scope

## Remote Schema Mapping Row

A remote schema mapping row must capture at least:
- mapping identity
- local schema identity
- mapping mode
- remote schema pattern
- include patterns
- exclude patterns
- rename rules
- last snapshot identity or epoch
- last donor capability class used with the mapping

## Remote Passthrough Policy Row

A remote passthrough policy row must capture admission flags for:
- query
- DML
- DDL
- admin
- procedural execution
- local join participation
- local transaction adjacency
- write fence requirement
- result disclaimer class

Policy must be explicit. Unsupported remote semantics must fail closed rather than being implicitly attempted.

## Migration Job Row

A migration row must capture:
- migration identity
- source connector identity
- target identity
- state
- throttle state
- mode version
- donor capability class
- extraction mode
- verification mode
- bytes and rows total
- bytes and rows copied
- unresolved drift count
- last error code and message
- verification or reconciliation epoch
- cutover readiness state

## Migration Extraction Window Row

For donor systems that do not support natural replication, durable change streams, or transaction forcing, the catalog model must additionally carry:
- extraction window identity
- extraction start and end markers
- watermark or cut-line identity
- overlap window policy
- consistency class
- compensation or replay policy
- validation pass identity

## Migration Chunk and Reconciliation Rows

The catalog model must provide row families for:
- chunk extraction progress
- verification pass summary
- mismatch summary
- unresolved drift classification
- cutover readiness assessment
- retirement or failback evidence

Each row must preserve stable identity for operator inspection and replay.

## Replication and Streaming Extension Rows

Where a connector or migration lane uses streaming or replication substrate, the catalog model must preserve:
- publication identity
- subscription identity
- channel identity and direction
- member identity and role
- origin identity and scope
- cursor progress
- batch receipt and apply state
- retry queue state
- redacted error surface

Secret-bearing strings must be redacted before catalog readback where the row is exposed outside privileged maintenance paths.

## Non-Negotiable Rule

A non-transactional or weakly transactional donor system must never be described as if it offered native MGA semantics. The catalog model must make the limitation explicit in committed metadata.

## Reconstructed Required Expansion

The rebuilt canon additionally requires:
- deterministic target-generation and cutover-readiness rows
- stable migration mismatch and drift rows queryable by admin SQL
- committed linkage between migration events, audit events, and routing changes
- policy-version capture for passthrough and cutover decisions
