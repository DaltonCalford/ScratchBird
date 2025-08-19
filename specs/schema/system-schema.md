---
id: system-schema
title: System Schema
status: draft
version: 0.1.0
updated: $(date -Iseconds)
related: []
---

### Overview
Catalog tables and monitoring views for metadata and runtime statistics. Align names and columns
with Firebird for compatibility.

### Acceptance (lexer only)
```
SELECT * FROM RDB$RELATIONS;
SELECT * FROM MON$ATTACHMENTS;
```
