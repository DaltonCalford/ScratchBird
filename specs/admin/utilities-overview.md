---
id: utilities-overview
title: Utilities Overview
status: draft
version: 0.1.0
updated: $(date -Iseconds)
related: []
---

## Summary
isql, backup/restore, stats, trace. Local file connections embed; remote connections proxy.

### Utilities
- isql: embedded vs remote passthrough, single-user lock for local files
- gfix: repair, sweep, shadowing
- gbak: backup/restore
- gstat/monitoring: stats and monitoring tables
- nbackup: incremental backups

### Acceptance (lexer only)
```
CONNECT 'localhost:/db.fdb';
```
