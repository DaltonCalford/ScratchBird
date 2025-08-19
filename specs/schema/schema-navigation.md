---
id: schema-navigation
title: Schema Navigation Commands
status: draft
version: 0.1.0
updated: $(date -Iseconds)
related: [schema-search-path]
---

## Summary
isql-style navigation: \\pwd_schema, \\cd_schema, \\ls_schema. Supports absolute and relative paths.

## Decisions
- Absolute path: org/app/prod.
- Relative: ., .., ../prod, ./sub.

## Acceptance Criteria
- Commands change session current schema and list children.
