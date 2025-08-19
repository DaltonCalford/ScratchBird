---
id: catalog-ids
title: Internal IDs in BLR & Plans
status: draft
version: 0.1.0
updated: $(date -Iseconds)
related: [catalog-objects]
---

## Summary
All compiled artifacts reference objects by internal ID; names are only for prepare-time resolution and diagnostics.

## Decisions
- Use UUIDs for cross-node mapping; keep numeric surrogates optional.
- Plan invalidation by catalog_version and dependency graph.
