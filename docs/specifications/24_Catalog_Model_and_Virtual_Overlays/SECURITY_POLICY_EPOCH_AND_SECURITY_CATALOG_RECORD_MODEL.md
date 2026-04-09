# Security Policy Epoch and Security Catalog Record Model

## Purpose

Define the catalog-side state that supports row security, column permissions, and domain security.

## Security Catalog Families

The current catalog model includes dedicated storage for:

- column-permission records
- row-level security policy records
- domain security records
- global security-policy epoch
- table-scoped policy epoch surfaces

## Epoch Model

Security-sensitive metadata changes shall advance epoch state so cached policy and authorization decisions can be invalidated.

Changes that require epoch advancement include:

- RLS enable or disable
- FORCE RLS changes
- policy create, drop, enable, or disable
- other security metadata changes that affect policy evaluation

## Domain Security Records

Domain security is persisted as a versioned binary payload. Corruption, unknown version, or malformed payload is a fail-closed condition.

## Column Permission Records

Column permission records are keyed by table and column name. Metadata maintenance operations, including column rename, shall keep these records synchronized with the canonical column identity.

## Policy Record Storage

Row-security policy records are first-class catalog records. Large expressions may use TOAST-backed storage rather than requiring inline-only policy payloads.

## Cache Coherence

All runtime security caches that depend on row policies, column permissions, or domain security shall track the relevant epoch or equivalent invalidation surface.

## Current Proof and Rebuild Boundary

Current code proves:

- dedicated catalog storage for column permissions
- dedicated row-security policy storage
- domain security binary persistence
- epoch bumping on table RLS state changes

This specification reconstructs the broader product rule that security metadata is versioned, cache-invalidating catalog authority rather than informal runtime-only state.
