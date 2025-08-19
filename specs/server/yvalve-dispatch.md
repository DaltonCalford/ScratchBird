---
id: yvalve-dispatch
title: Y-Valve Dispatch
status: draft
version: 0.1.0
updated: $(date -Iseconds)
related: [providers]
---

## Summary
Dispatch connection requests to providers: embedded, remote, legacy.

## Decisions
- Embedded for local files; remote for host: URIs; legacy for compat modes.
