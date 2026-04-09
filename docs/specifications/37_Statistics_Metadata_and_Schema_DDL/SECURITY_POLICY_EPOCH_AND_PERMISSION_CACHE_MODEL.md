# Security Policy Epoch and Permission Cache Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the committed security publication anchors and the cache-validation rules for permission-sensitive metadata answers.

## Current code-backed proof

Current code-backed state proves:
- a global security policy epoch
- a per-table policy epoch
- connection-context storage for current and pending security epochs
- permission-cache entries keyed by global and table policy epoch
- table metadata carrying `policy_epoch`

Current permission-cache runtime also proves:
- bounded cache size
- TTL expiration
- LRU eviction
- cache-disable mode
- security-quorum gating before cached authorization is trusted

## Canonical model

Security-sensitive caches must validate against:
- committed global security policy epoch
- committed table policy epoch when the answer is table-scoped
- object identity and transaction visibility where applicable

## Publication algorithm

When committed security metadata changes:
1. mutate catalog rows transactionally
2. advance global security epoch and table policy epoch as required by the change scope
3. persist the new epoch state atomically with committed metadata publication
4. expose the new anchors to connection context and downstream caches only after commit

## Cache invalidation rules

- a cache miss may populate against the current committed security anchors
- a cache hit is valid only if both stored anchors match the current committed anchors relevant to the answer
- if anchors differ, the cache answer is stale and must be discarded
- rollback does not publish new security anchors

Additional current rules recovered from code:
- non-table objects may validate only against the global security policy epoch
- table objects must validate against both global and table policy epochs
- if current epochs cannot be loaded safely, the cache path must be bypassed
- security-quorum `DENY` must fail closed before cache lookup
- security-quorum `ALLOW_CACHE` is required before a cached answer may be trusted

## Consumers

The following classes must honor security epoch validation when they make authorization-sensitive decisions:
- permission caches
- discoverability caches
- parser metadata mirrors used for security-sensitive inspection
- plan or render caches when security policy version participates in semantics

## Required operator outputs

Commercial-grade canon requires the engine to preserve at minimum:
- total lookups
- hit count
- miss count
- eviction count
- invalidation count
- TTL expiration count
- current entries
- configured max entries

These counters are part of the permission-cache observability contract and must remain attributable to committed security epochs rather than vague cache-health claims.
