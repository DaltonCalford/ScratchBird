Status: reconstructed_required_with_current_substrate

# PREPARED STATEMENT AND TRANSLATION CACHE IDENTITY INVALIDATION AND EPOCH MODEL

## Purpose

This file defines the canonical memory and invalidation contract for the
process-local statement-cache and translation-cache families.

These caches are distinct from:

- executor result cache
- pool result cache
- permission cache
- buffer and page residency

They must not be collapsed into a generic "plan cache" with no identity or
epoch discipline.

## Current code-backed boundary

Current recovered code authority already proves:

- ScratchBird has a statement-cache implementation
- ScratchBird has a translation-cache family separate from result caching
- these cache families are process-local reusable artifacts
- they are derivative, not authoritative truth
- cache invalidation is not one undifferentiated global event type

This file therefore treats the existence of these cache families as current
authority, even where some deeper admission details are still under rebuild.

## Governing rule

Prepared statement and translation cache entries are reusable only while all
planner-shaping, schema-shaping, and security-shaping identity inputs remain
compatible with the cached artifact.

Compatibility is not determined by SQL text alone.

## Cache-family separation

### Statement cache family

The statement cache owns reusable prepared-statement style artifacts that have
already crossed parser and validation boundaries and are suitable for repeated
execution only when the execution-shaping environment remains compatible.

### Translation cache family

The translation cache owns reusable canonical lowering or translation artifacts
that sit between parser-local SQL handling and later planning or execution
stages.

The translation cache is not the same thing as:

- a rendered SQL cache
- a result cache
- a permission-decision cache

## Required identity envelope

The rebuilt specification requires statement-cache and translation-cache keys to
bind, at minimum, the following classes of identity:

1. canonical statement or translation surface
2. parser family and parser-local capability profile
3. dialect or emulation mode
4. canonical V3 or SBLR version identity where relevant
5. schema epoch or equivalent dependency signature
6. security-policy epoch where authorization or visible semantics can differ
7. search-path or schema-visibility context when resolution depends on it
8. plan-affecting session settings
9. object-dependency signature when explicit dependency tracking exists

A cache key that ignores these classes is under-specified.

## Epoch discipline

Prepared statement and translation caches are subordinate to:

1. committed schema state
2. committed dependency state
3. committed security and policy state
4. current parser capability and session setting surface

Therefore, reuse must stop when any of the following changes:

- schema epoch
- dependency signature
- security-policy epoch
- relevant search-path or home-schema binding
- parser feature or capability profile affecting the lowered meaning

## MGA rule

Because ScratchBird is always in a transaction, prepared statement and
translation reuse must remain compatible with transaction-bound catalog and
security visibility.

These caches may accelerate repeated work, but they must never let a session
observe:

- stale object identity
- stale privilege shape
- stale row-policy semantics
- stale search-path or schema binding

## Current-versus-required boundary

### Current authority

Current code authority is sufficient to say:

- statement and translation caching exist
- they are separate cache families
- they are derivative and invalidate on more than one event type

### Required reconstructed behavior

The rebuilt commercial-grade specification additionally requires:

- explicit key identity fields for every cache family
- explicit invalidation reason classes
- explicit epoch or dependency binding
- explicit operator observability separating statement-cache events from
  translation-cache events
- explicit fail-closed reuse refusal when compatibility cannot be proven

## Invalidation reasons

The canonical invalidation reasons for statement and translation caches must
include at least:

1. schema invalidation
2. dependency invalidation
3. security invalidation
4. parser-capability invalidation
5. session-setting invalidation
6. memory-pressure eviction
7. operator flush

If the engine cannot classify the reason precisely, it must still refuse unsafe
reuse.

## Reuse rules

A statement or translation artifact may be reused only when:

1. the canonical statement identity matches
2. the parser and dialect identity matches
3. schema and dependency state are compatible
4. security and visibility state are compatible
5. plan-affecting settings are compatible
6. the artifact has not been invalidated, retired, or quarantined

## Fail-closed boundary

The following are non-conforming:

1. reusing a prepared statement after schema epoch change without proving
   dependency compatibility
2. reusing translation output across incompatible parser or emulation settings
3. treating security epoch change as irrelevant to prepared or translated
   artifacts
4. allowing statement-cache reuse when search-path or schema-binding context has
   changed materially
5. describing statement and translation cache hits as equivalent to result-cache
   hits

## Observability requirement

Operators must eventually be able to distinguish:

- statement-cache hit, miss, invalidate, and evict activity
- translation-cache hit, miss, invalidate, and evict activity
- combined generic cache activity

Until public counters are split fully, combined counters are not sufficient
proof of healthy prepared-statement reuse.
