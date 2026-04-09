# SBLR_TO_V3_NATIVE_SQL_RENDER_ENDPOINT_AND_NAME_RESOLUTION_MODEL

## Status

Current code-backed authority with reconstructed target-language clarification.

## Purpose

This document defines the engine-internal endpoint that exposes deterministic SBLR-to-native/v3 SQL rendering for trusted language UDR and inspection flows.

## Governing rule

The endpoint renders canonical native/v3 SQL only.

It does not promise original dialect recovery.

## Request contract

The current render-endpoint request includes:

1. request ID
2. profile ID
3. profile version
4. native feature key
5. principal ID
6. role-context signature
7. render-permission flag
8. root instruction

## Response contract

The current response includes:

1. request ID
2. success flag
3. native feature key
4. SQL text
5. contract ID
6. canonical opcode symbol
7. diagnostic list

## Registry and feature-gating rules

Current code-backed endpoint behavior requires:

1. a registered language UDR module
2. the render feature to be enabled for that module
3. an eligible profile
4. render permission to be granted

If those conditions are not met, the endpoint rejects the request.

## Current diagnostic boundary

Current tests prove specific fail-closed rejection lanes, including:

1. feature not enabled
2. render permission denied
3. malformed request envelope
4. unknown profile
5. unrenderable instruction

The endpoint therefore exists as a permission-gated, diagnosable render surface rather than a best-effort stringifier.

## Name-resolution path

The render path may use catalog-backed UUID-to-name resolution.

Current name resolution is:

1. optional
2. bounded by object-type hint
3. catalog-backed
4. non-fabricating when a name cannot be resolved

## Determinism rule

Repeated endpoint render of the same eligible request must produce:

1. the same SQL text
2. the same contract ID
3. the same canonical opcode symbol

## Relationship to parser isolation

This endpoint does not violate the parser-isolation rule.

Why:

1. parser isolation governs SQL-to-SBLR lowering
2. this endpoint is a post-lowering canonical render surface
3. it emits canonical native/v3 SQL, not parser-specific dialect output

## Required implementer interpretation

Another agent shall preserve:

1. feature gating
2. permission gating
3. deterministic output
4. canonical native/v3 target only
5. optional but bounded catalog name resolution
