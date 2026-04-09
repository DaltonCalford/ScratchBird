# Access Method DDL DML and Maintenance Interaction

Status: current_authority

## Purpose

Define how current access methods interact with DDL, DML, uniqueness checks, fragmentation advice, cleanup publication, and maintenance evidence.

## Current DML Interaction

Current storage-layer DML already includes:
- tuple insert
- tuple delete
- tuple update
- delete-by-session for temp tables
- index-candidate filtering by visible heap truth
- unique preflight on insert and update
- stable-TID-aware index mutation

This means DML publication into access paths is not generic folklore. It is tied to explicit storage-engine mutation helpers.

## Current Index Mutation Boundary

Current runtime already uses family-specific helper integration for:
- index remove
- candidate filtering against visible heap tuples
- stable-TID updates during mutation

The access-method interaction model is therefore:
- MGA-first
- heap-truth first
- family-dispatched second

## DDL Interaction

Current DDL interaction remains catalog and metadata driven.

That means:
- create, alter, and drop semantics depend on catalog truth
- access-method runtime objects must follow committed metadata publication
- no file or runtime sidecar may become authoritative before catalog publication

## Maintenance and Advisory Publication

Current storage runtime already publishes:
- fragmentation advisories
- index cleanup publication records

Those are current evidence surfaces, not planning placeholders.

Required interpretation:
- fragmentation advice is operator and maintenance evidence
- cleanup publication is explicit maintenance work evidence
- neither widens transaction truth or durability truth

## Rebuild and Verification Boundary

Rebuild, verification, and cleanup semantics are family specific.

Section `34` may state the shared interaction rules:
1. heap-visible truth drives cleanup eligibility
2. cleanup publication must not outrun matching heap proof
3. rebuild and verification remain family-specific and section `18` owned

## Explicit Non-Guarantees

- no universal online DDL contract
- no uniform rebuild semantics across all methods
- no general autonomous maintenance subsystem guarantee
