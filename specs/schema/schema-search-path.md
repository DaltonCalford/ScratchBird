---
id: schema-search-path
title: Schema Search Path & Homes
status: draft
version: 0.1.0
updated: $(date -Iseconds)
related: [schema-navigation, catalog-objects]
---

## Summary
Users and roles have home schemas; sessions have a current schema and a search path. Name resolution uses the search path (head→tail).

## Decisions
- Effective search path order: current → user home → role homes (priority) → db default → public.
- SET SCHEMA prepends; SET SEARCH_PATH replaces (STRICT disables homes append).
- Shadowing: earlier path entries hide later ones.

## Acceptance Criteria
- Unqualified name resolves against path in order.
- \\cd_schema updates current schema; relative paths (., ..) work.

## Open Questions
- Should role home order be deterministic by grant time or explicit priority?
