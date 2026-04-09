# WAL-After Log Shipping and Debug Scope

Status: current_authority

## Current authority

Current proof is limited to sweep-manager debug, export, page-audit, and evidence lanes carrying `WAL_AFTER_*` policy vocabulary.

## Current guarantees

- `WAL_AFTER_*` is derivative, post-commit, and optional
- derivative export failure does not redefine MGA durability truth
- this surface may be used for debug, audit, and export purposes where current code proves it

## Non-claims

- core recovery architecture
- cluster log replication
- commit-index or consensus semantics
